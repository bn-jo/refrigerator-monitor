# System Architecture

Industrial Refrigerator Monitor — WT32-ETH01 (ESP32 + LAN8720).

---

## 1. System block diagram

```
        ┌──────────────────────────────────────────────────────────────┐
        │                          WT32-ETH01 (ESP32)                    │
        │                                                                │
   5×DS18B20 ─OneWire(GPIO4)─►┐                                          │
   1×DS18B20 ─OneWire(GPIO14)►│                                          │
        │                     ▼                                          │
        │              ┌─────────────┐   once/min   ┌────────────────┐  │
        │              │  Sensors    │─────────────►│  Acquisition    │  │
        │              │ (DallasTemp)│              │  Task (core 1)  │  │
        │              └─────────────┘              └───────┬────────┘  │
        │                                                   │           │
        │            ┌──────────────┬─────────────┬─────────┼────────┐  │
        │            ▼              ▼             ▼          ▼        │  │
        │      ┌──────────┐  ┌───────────┐  ┌──────────┐ ┌────────┐  │  │
        │      │ Storage  │  │  Alarms   │  │  Logger  │ │ Email  │  │  │
        │      │ circular │  │ hysteresis│  │ rotating │ │ SMTP   │  │  │
        │      │ DB(LFS)  │  │ + anti-   │  │ events   │ │ TLS    │  │  │
        │      └────┬─────┘  │  spam     │  └────┬─────┘ └───┬────┘  │  │
        │           │        └─────┬─────┘       │           │       │  │
        │           ▼              ▼             ▼           │       │  │
        │      ┌────────────────────────────────────────┐   │       │  │
        │      │     Async Web Server (port 80)          │   │       │  │
        │      │  static UI + REST API + auth/session    │   │       │  │
        │      └───────────────┬─────────────────────────┘   │       │  │
        │   ┌──────────┐  ┌─────┴──────┐  ┌──────────┐        │       │  │
        │   │ TimeSync │  │  Network   │  │ Watchdog │◄───────┘       │  │
        │   │  NTP/TZ  │  │ ETH+WiFi   │  │ HW + SW  │ heartbeat      │  │
        │   └──────────┘  │  fallback  │  └──────────┘                │  │
        │                 └─────┬──────┘                              │  │
        └───────────────────────┼─────────────────────────────────────┘
                                │ RJ45 (DHCP)        │ SMTP
                                ▼                    ▼
                          LAN / Router          Mail server
                                │
                                ▼
                  Browser (desktop / tablet / mobile)
```

---

## 2. Software architecture

The firmware is **modular**, one responsibility per translation unit. Each
module is a singleton with a small, explicit API.

| Module        | File(s)              | Responsibility |
|---------------|----------------------|----------------|
| Config        | `config.h`           | All compile-time constants & pin map |
| Logger        | `Logger.*`           | Rotating event log to LittleFS + serial mirror, thread-safe |
| TimeSync      | `TimeSync.*`         | SNTP, POSIX timezone, ISO timestamps, uptime |
| Settings      | `Settings.*`         | JSON config in flash, atomic save, password hashing |
| Network       | `Network.*`          | Ethernet bring-up, WiFi fallback FSM, Internet probe |
| Sensors       | `Sensors.*`          | DS18B20 discovery/addressing, non-blocking reads, fault classification |
| Storage       | `Storage.*`          | Fixed-size circular history DB with CRCs |
| Alarms        | `Alarms.*`           | Threshold + hysteresis + confirmation + anti-spam |
| Email         | `Email.*`            | SMTP alarm/recovery/test messages (HTML) |
| Auth          | `Auth.*`             | Session tokens, idle timeout |
| WebServer     | `WebServer.*`        | Async HTTP: static files + REST API |
| Watchdog      | `Watchdog.*`         | Hardware TWDT + software heartbeat supervision |
| main          | `main.cpp`           | Ordered bring-up, acquisition task, supervisory loop |

### Concurrency model
- **Core 1 — Acquisition task**: every 60 s it triggers conversions, waits
  ~800 ms, reads sensors, persists to storage, and runs alarm logic. Heavy /
  blocking work (flash writes, SMTP) lives here so it never stalls the web
  server.
- **Core 0 — `loop()` + AsyncTCP**: the web server is fully asynchronous
  (event-driven, no per-request thread). `loop()` services the network FSM,
  NTP, watchdog, session sweeping and the deferred restart.
- Shared state (history file, logs) is guarded by FreeRTOS mutexes.

---

## 3. Data storage

### Chosen solution: pre-allocated **circular binary database on LittleFS**

**Why not the alternatives:**
- **SD card** — best raw capacity, but adds a hardware failure point (card
  corruption, contact issues in a cold/damp environment) and consumes scarce
  WT32-ETH01 GPIO/SPI pins that conflict with Ethernet. Rejected for
  reliability.
- **SPIFFS** — deprecated, no true directories, poor wear behaviour, slower.
  LittleFS supersedes it (power-loss resilient, better wear levelling).
- **SQLite** — feasible on ESP32 with PSRAM, but heavy: large RAM/flash
  footprint, journal/WAL files that can fragment flash, and overkill for a
  pure time-series append workload. Rejected as over-engineered.
- **LittleFS circular DB (chosen)** — minimal, deterministic, power-safe.

**Design:**
- One file `/history.db` is **pre-allocated** to its full final size at first
  boot. It never grows, so there is **no "disk full" failure mode** and flash
  wear is spread evenly across the partition.
- It is a **ring buffer** of fixed records. A small header tracks `head` and
  `count` with a CRC32; each record carries a **CRC8**.

```
record = { uint32 ts; int16 temp[5] (centi-°C); uint8 crc8 }   // 15 bytes
```

### Storage estimate
- Sampling: 1/min ⇒ 1 440 records/day ⇒ **43 200 records / 30 days**.
- 43 200 × 15 B ≈ **633 KB** + header. The LittleFS partition is **896 KB**
  (see `partitions.csv`), leaving room for the web UI (~40 KB) and logs.

### Retention & rotation
- The ring **automatically overwrites the oldest record** once full → a
  rolling 30-day window with **no manual log rotation needed** for history.
- The **event log** (`/events.log`) rotates at 128 KB to a single previous
  generation (`events.log.1`), capping total log flash use.

### Data integrity
- Each record's CRC8 is verified on read; corrupt records (e.g. half-written
  during power loss) are **skipped**, not served as bad data.
- The header is rewritten (`flush`/fsync) **after** the data write commits,
  so a crash mid-write loses at most the single in-flight sample.

### Recovery after power loss
- On boot the header is validated by CRC. If valid, the ring resumes exactly.
- If the header is corrupt, `rebuildFromScan()` reconstructs `head`/`count`
  by scanning valid records and their timestamps. **No data loss** beyond the
  one possibly-incomplete record.
- Settings use **atomic write** (temp file + rename), so a crash never leaves
  a half-written config.

---

## 4. Reliability & fault tolerance

| Failure mode             | Detection                          | Response |
|--------------------------|------------------------------------|----------|
| Task lockup              | Hardware Task WDT (30 s)           | Panic + reset |
| Acquisition stall        | Software WDT (no heartbeat > 3 cycles) | Clean logged restart |
| Unhandled exception      | ESP32 panic handler + coredump partition | Reset; coredump kept |
| Ethernet unplugged       | ETH link/IP events                 | Log; auto-reconnect; WiFi fallback after 15 s |
| Internet loss            | Periodic TCP probe to 1.1.1.1:53   | Log + dashboard flag |
| DS18B20 comms error      | DEVICE_DISCONNECTED / CRC          | Retry ×3, classify FAULT, log |
| Missing sensor           | Search ROM count < 5               | Mark MISSING, re-discover each cycle |
| Invalid / 85 °C reading  | Plausibility window + 85 °C filter | Reject, not stored/alarmed |
| Out-of-range temperature | TEMP_MIN/MAX_VALID                 | Classify OUTOFRANGE |
| Storage write failure    | Return code + counter              | Log, surfaced via `/api/status` |
| Power loss               | CRCs + atomic writes               | Resume ring, rebuild if needed |

### Safe startup sequence (`setup()`)
1. Serial → 2. LittleFS (format-on-fail) → 3. Logger + Settings + TimeSync →
4. Storage → 5. Sensors → 6. Alarms/Email/Auth → 7. Network → 8. WebServer →
9. Watchdog → spawn acquisition task. Each step logs success/failure so a
boot loop is diagnosable from the serial console or the persisted log.

---

## 5. Alarm logic (anti-spam)

```
state NORMAL ──(t>thr for N consecutive reads)──► ALARM  → send immediate e-mail
ALARM ──(every 30 min while active)──────────────► reminder e-mail
ALARM ──(t ≤ thr − hysteresis)───────────────────► NORMAL → send recovery e-mail
```

- **Confirmation count** (`ALARM_CONFIRM_COUNT`) prevents single-sample noise
  from raising alarms.
- **Hysteresis** (`ALARM_HYSTERESIS_C`) prevents flapping around the
  threshold.
- Dashboard colour: **green** normal, **yellow** within 1 °C of threshold,
  **red** over threshold.
