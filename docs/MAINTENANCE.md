# Maintenance & Operations

## Routine checks

| Interval   | Task |
|------------|------|
| Daily      | Glance at dashboard: all tiles green, Ethernet + Internet ✓, time synced. |
| Weekly     | Review **Logs** for repeated sensor faults or network drops. Confirm a test e-mail still arrives. |
| Monthly    | Verify each physical probe reads plausibly; compare against a reference thermometer. Check cable strain relief. |
| Quarterly  | Confirm history spans 30 days; export logs for records. Re-test alarm by warming one probe. |
| Annually   | Re-seat connectors; inspect for corrosion/condensation; update firmware. |

## Common procedures

### Identify which physical sensor is which
Each tile shows its **ROM ID**. Warm one probe by hand; the tile whose temp
rises identifies that sensor. Rename it in **Settings** and label the cable.

### Change a temperature threshold or name
Settings → edit the fridge row → **Save settings**. Persists across reboots.

### Add / replace a sensor
Wire the new DS18B20 onto the bus (same VDD/GND/DATA). Reboot (or it is
re-discovered automatically next cycle). Because addresses are sorted, adding
a sensor with a lower ROM ID can shift slot order — re-verify names/labels
after adding hardware.

### Reset the admin password
If locked out, re-flash with a one-time default, or erase NVS + LittleFS:
```bash
pio run -t erase           # erases flash; settings return to defaults (admin/admin)
```
Then re-upload firmware and filesystem.

### Restart the device
Dashboard → **Restart Device** (with confirmation), or `POST /api/restart`.
The event is logged before reboot.

### Offline operation (no Internet for Chart.js)
The UI loads Chart.js from a CDN by default. For sites without Internet:
1. Download into `data/js/`:
   - `chart.umd.min.js` (Chart.js v4)
   - `hammer.min.js`
   - `chartjs-plugin-zoom.min.js`
2. In `data/index.html`, change the three `<script src="https://…">` tags to
   `/js/chart.umd.min.js`, `/js/hammer.min.js`, `/js/chartjs-plugin-zoom.min.js`.
3. Re-upload the filesystem image. Everything else already runs locally on
   the ESP32, so the device is fully self-hosted.

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| Board won't boot after wiring | Pull-up on a strapping pin — use GPIO4/14 as documented. |
| A fridge shows `missing` | Sensor not enumerated: check DATA/GND continuity, pull-up present, connector seated. |
| A fridge shows `fault` / 85 °C | Power/conversion glitch: use external power, add 100 nF at sensor, shorten stub. |
| All sensors `missing` | Wrong GPIO in `config.h`, missing pull-up, or no 3.3 V to sensors. |
| No IP / `mode` stuck | Check RJ45 link LED, DHCP on router; serial log shows ETH events. Falls back to WiFi `ben` after 15 s. |
| Internet ✕ but LAN ok | Firewall blocking outbound 53; e-mail/NTP may fail. |
| E-mail not sent | Check SMTP host/port/credentials; Gmail needs an **App Password**; see Logs. |
| Charts empty | Not enough history yet, or Chart.js CDN blocked (see offline section). |
| Frequent restarts | Check Logs for "software watchdog" (acquisition stall) or panic/coredump. |

## Firmware / FS update
- Code change → `pio run -t upload` (USB) or `pio run -e ota -t upload` (network).
- Web UI change (anything in `data/`) → `pio run -t uploadfs` (USB) or
  `pio run -e ota -t uploadfs` (network).

### OTA notes
- OTA needs the device already running networked firmware; the first flash
  must be over USB.
- Keep `[env:ota] upload_port` = device static IP and `--auth=` = `OTA_PASSWORD`.
- An OTA **firmware** update preserves LittleFS (history/settings/logs). An
  OTA **`uploadfs`** still rewrites the whole filesystem partition (erases
  history/settings/logs) — same caveat as USB `uploadfs` below.
- A **code upload** (`upload`) does *not* touch the LittleFS partition, so
  history, settings and logs are preserved.
- An **`uploadfs`** rewrites the *entire* LittleFS partition and therefore
  **erases `/history.db`, `/settings.json` and `/events.log`**. Re-configure
  settings afterward, and expect history to start fresh. Only run `uploadfs`
  when the web UI itself changed.
