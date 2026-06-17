# REST API

All responses are JSON. All endpoints except `/api/login` require a valid
session cookie (`FMSESSION`), set by logging in. Replace `192.168.1.50`
with your device IP.

---

## Authentication

### `POST /api/login`
```bash
curl -i -X POST http://192.168.1.50/api/login \
  -H 'Content-Type: application/json' \
  -d '{"user":"admin","pass":"admin"}'
```
On success returns `{"ok":true}` and a `Set-Cookie: FMSESSION=…` header.
Save the cookie for subsequent calls:
```bash
curl -c cookies.txt -X POST http://192.168.1.50/api/login \
  -H 'Content-Type: application/json' -d '{"user":"admin","pass":"admin"}'
curl -b cookies.txt http://192.168.1.50/api/status
```

### `POST /api/logout`
Invalidates the current session.

### `GET /api/authcheck` → `{"authed":true|false}`

---

## `GET /api/status`
System and connectivity health.
```json
{
  "fw":"1.0.0","build":"Jun 16 2026 14:00:00","device":"fridge-monitor",
  "mode":"Ethernet","ethUp":true,"wifi":false,"internet":true,
  "ip":"192.168.1.50","mac":"AA:BB:CC:DD:EE:FF",
  "time":"2026-06-16 14:03:21","timeSynced":true,
  "tz":"IST-2IDT,M3.4.4/26,M10.5.0","uptime":"3d 04:11:27",
  "heapFree":182340,"records":43200,"storageOk":true,"writeFails":0,
  "dataAcqOk":true,"lastAcq":1781999001,"alarmActive":false
}
```

## `GET /api/temperatures`
Live readings for all six sensors (index 5 = outdoor).
```json
{ "sensors":[
  {"index":0,"name":"Milk Refrigerator","outdoor":false,"temp":3.6,
   "sensor":"ok","romId":"28FF1A2B3C4D5E6F","lastValid":1781999001,
   "threshold":4.0,"status":"normal","enabled":true},
  {"index":5,"name":"Outdoor","outdoor":true,"temp":27.4,
   "sensor":"ok","romId":"28AA...", "lastValid":1781999001}
]}
```
`status` ∈ `normal|near|alarm`; `sensor` ∈ `ok|fault|missing|outofrange`.

## `GET /api/history?range=1h|24h|7d|30d`
Server-downsampled series for the 5 refrigerators (smooth on mobile).
```bash
curl -b cookies.txt 'http://192.168.1.50/api/history?range=7d'
```
```json
{ "from":1781912601,"to":1781999001,"bucket":1800,
  "points":[ {"t":1781912601,"v":[3.6,5.1,4.8,2.9,3.7]}, ... ] }
```
`v` is the per-bucket average for fridges 0–4; `null` where no valid data.

## `GET /api/logs?level=ERROR&cat=net`
Returns the most recent log lines (filtered) as a JSON string array.
```json
["2026-06-16 14:00:01 [INFO] system: system boot — firmware v1.0.0", ...]
```

## `GET /api/logs/download`
Streams the raw `events.log` as a file download.

---

## `POST /api/settings`
Partial update; omitted fields are unchanged. Empty `email.pass` keeps the
stored password. `newPassword` (≥6 chars) changes the login password.
```bash
curl -b cookies.txt -X POST http://192.168.1.50/api/settings \
  -H 'Content-Type: application/json' -d '{
    "fridges":[
      {"name":"Milk Refrigerator","thr":4,"en":true},
      {"name":"Meat Refrigerator","thr":6,"en":true},
      {"name":"Vegetable Refrigerator","thr":5,"en":true},
      {"name":"Refrigerator 4","thr":3,"en":true},
      {"name":"Refrigerator 5","thr":4,"en":true}],
    "outdoorName":"Outdoor","tz":"IST-2IDT,M3.4.4/26,M10.5.0",
    "email":{"enabled":true,"host":"smtp.gmail.com","port":465,
             "user":"alerts@example.com","pass":"app-password",
             "sender":"alerts@example.com","recipient":"manager@example.com"}
  }'
```
→ `{"ok":true}` or `{"error":"..."}`.

## `POST /api/email/test`
Sends a test e-mail using the stored SMTP settings → `{"ok":true|false}`.

## `POST /api/logs/clear`
Clears all event logs.

## `POST /api/restart`
Schedules a safe restart (response is sent first, then the device reboots).
```bash
curl -b cookies.txt -X POST http://192.168.1.50/api/restart
```
