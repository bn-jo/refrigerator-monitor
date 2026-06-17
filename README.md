# ❄️ Industrial Refrigerator Monitor (ESP32-ETH01)

A production-ready, 24/7 monitoring system for commercial refrigerators built
on the **WT32-ETH01 (ESP32 + LAN8720 Ethernet)**. It monitors **5 refrigerator
DS18B20 sensors + 1 outdoor sensor**, stores **30 days** of history, serves a
modern responsive web app, raises **e-mail alarms**, and is designed for fault
tolerance with hardware/software watchdogs and automatic recovery.

| Feature | Summary |
|---------|---------|
| Sensors | 5× DS18B20 on one OneWire bus (GPIO4) + 1× outdoor (GPIO14) |
| Sampling | Once per minute |
| Storage | Pre-allocated **circular DB on LittleFS**, CRC-protected, 30-day window |
| UI | Responsive HTML/CSS/JS, Chart.js history, dashboard, settings, logs |
| Alerts | SMTP e-mail: immediate + 30-min reminders + recovery, anti-spam |
| Networking | Ethernet (DHCP) with **WiFi fallback**, Internet reachability probe |
| Reliability | HW Task WDT + SW watchdog, safe boot, power-loss-safe storage |
| Security | Login, salted **SHA-256** password, session cookies + idle timeout |
| Time | NTP + configurable POSIX timezone, timestamps on all data/logs |
| API | JSON REST (`/api/status`, `/temperatures`, `/history`, `/logs`, `/settings`, `/restart`) |

> **Full design docs:** [Architecture](docs/ARCHITECTURE.md) ·
> [Wiring](docs/WIRING.md) · [REST API](docs/API.md) ·
> [Maintenance](docs/MAINTENANCE.md)

---

## Folder structure

```
refrigerator monitor/
├── platformio.ini          # build config, board, libraries
├── partitions.csv          # 4MB flash layout (OTA + 896KB LittleFS)
├── README.md
├── docs/
│   ├── ARCHITECTURE.md      # software arch, block diagram, storage, reliability
│   ├── WIRING.md            # DS18B20 multi-drop, pin table, cable, noise
│   ├── API.md               # REST endpoints + curl examples
│   └── MAINTENANCE.md       # ops, troubleshooting, offline mode
├── src/                     # ESP32 firmware (C++)
│   ├── main.cpp             # bring-up, acquisition task, supervisory loop
│   ├── config.h             # pins & all tunables
│   ├── Logger.*             # rotating event log
│   ├── TimeSync.*           # NTP / timezone
│   ├── Settings.*           # persistent JSON config + password hashing
│   ├── Network.*            # Ethernet + WiFi fallback + Internet probe
│   ├── Sensors.*            # DS18B20 discovery/addressing/reads
│   ├── Storage.*            # circular history database
│   ├── Alarms.*             # thresholds, hysteresis, anti-spam
│   ├── Email.*              # SMTP notifications
│   ├── Auth.*               # sessions
│   ├── WebServer.*          # async HTTP: UI + REST API
│   └── Watchdog.*           # HW + SW watchdog
└── data/                    # web UI (flashed to LittleFS)
    ├── index.html  login.html
    ├── css/style.css
    └── js/ app.js charts.js settings.js logs.js login.js
```

---

## Toolchain setup

### Option A — PlatformIO (recommended)
1. Install [VS Code](https://code.visualstudio.com/) + the **PlatformIO IDE**
   extension (bundles the toolchain).
2. Open this folder. PlatformIO reads `platformio.ini` and installs the ESP32
   platform and all libraries automatically on first build.

### Option B — Arduino IDE
1. Install **ESP32 board support** (Boards Manager → "esp32" by Espressif).
2. Select board **"WT32-ETH01"**.
3. Tools → Partition Scheme: pick a scheme with ≥1 MB SPIFFS/LittleFS (or use
   the provided `partitions.csv`), filesystem = LittleFS.
4. Install libraries (below) via Library Manager.
5. Use the **ESP32 LittleFS Data Upload** plugin to flash the `data/` folder.

### Required libraries
| Library | Purpose |
|---------|---------|
| OneWire (PaulStoffregen) | 1-Wire bus |
| DallasTemperature (milesburton) | DS18B20 |
| ArduinoJson (bblanchon) ≥7 | JSON |
| ESP-Mail-Client (mobizt) | SMTP e-mail |
| ESPAsyncWebServer (esp32async) | async HTTP server |
| AsyncTCP (esp32async) | async TCP backend |

(mbedTLS, LittleFS, ETH, WiFi, esp_task_wdt ship with the ESP32 core.)

---

## Build & flash

```bash
# Build firmware
pio run

# Flash firmware over USB
pio run -t upload

# Build + flash the web UI (LittleFS image) — run once, and after UI changes
pio run -t uploadfs

# Serial monitor (115200)
pio device monitor
```

### Static IP
The device uses a **fixed IP** so the UI is always reachable at a known
address. Edit the octets in `src/config.h` to match your LAN **before
flashing** (set `USE_STATIC_IP 0` to use DHCP instead):

```c
#define STATIC_IP_OCTETS     192,168,1,50    // device address
#define STATIC_GW_OCTETS     192,168,1,1     // gateway
#define STATIC_MASK_OCTETS   255,255,255,0   // subnet
```

> The address must be inside your router's subnet and ideally **outside its
> DHCP pool** to avoid conflicts. It applies to both Ethernet and the WiFi
> fallback. Then browse to `http://192.168.1.50/`.

### OTA (over-the-air) updates
After the **first** USB flash, update the device wirelessly over the network
— no serial adapter or bootloader jumper needed:

```bash
pio run -e ota -t upload      # firmware
pio run -e ota -t uploadfs    # web UI
```

Keep `upload_port` in `[env:ota]` (platformio.ini) equal to the device's
static IP, and `--auth=` equal to `OTA_PASSWORD` in `config.h`. **Change the
default OTA password before deployment.** During an update the acquisition
task is suspended and the watchdog is fed automatically; the device reboots
into the new image when finished.

> The WT32-ETH01 has **no onboard USB**: connect a **3.3 V USB-TTL adapter**
> to **TX0/RX0/GND** (cross TX↔RX). To enter flash mode, hold **IO0 to GND**
> while powering up / pressing EN, then release after upload starts. Some
> adapters with DTR/RTS auto-reset this for you.

### First boot
- Default login: **`admin` / `admin`** — change it immediately in Settings.
- Configure SMTP, fridge names, thresholds, and timezone in Settings.

---

## Finding the device IP

By default the device uses the **static IP** set in `config.h`
(`192.168.1.50` out of the box) — just browse to `http://192.168.1.50/`. The
serial monitor also prints `Ethernet static IP …` at boot to confirm.

If you switched to DHCP (`USE_STATIC_IP 0`):
- **Serial monitor** — prints `Ethernet got IP x.x.x.x` at boot.
- **Router admin page** — look for hostname **`fridge-monitor`** in the DHCP
  client list (and assign a DHCP reservation for a permanent address).
- **Network scan** — `ping fridge-monitor.local` (mDNS) or `nmap -sn 192.168.1.0/24`.

---

## Reliability summary

- **Watchdogs:** hardware Task WDT (30 s) resets on lockups; a software
  watchdog restarts cleanly if acquisition stalls.
- **Network recovery:** Ethernet auto-reconnects; falls back to WiFi
  (`ben`) if no link in 15 s; prefers Ethernet again when it returns.
- **No data loss on power failure:** CRC-protected circular DB + atomic
  settings writes; the ring is rebuilt by scan if the header is damaged.
- **Fault visibility:** sensor/network/storage faults appear on the
  dashboard, in the event log, and (where appropriate) by e-mail.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full reliability
matrix and storage design rationale.

---

## Future expansion

- **OTA updates** — partition table already reserves dual OTA app slots; add
  `Update`/ArduinoOTA or a web-upload endpoint.
- **HTTPS** — terminate TLS on-device or behind a reverse proxy.
- **MQTT / cloud** — publish readings to an MQTT broker or time-series DB
  (InfluxDB/Grafana) for fleet dashboards.
- **More sensors / relays** — door switches, humidity (SHT3x), compressor
  current; drive a relay to cut power or sound a siren on alarm.
- **SMS / push** — add Twilio or Telegram alerts alongside e-mail.
- **Multi-user roles** — extend the auth module with read-only accounts.
- **Per-sensor calibration offsets** and **min/max daily summaries**.
