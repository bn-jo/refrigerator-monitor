/* =====================================================================
 *  secrets.example.h — template for local secrets.
 *
 *  Copy this file to  src/secrets.h  and fill in your own values.
 *  src/secrets.h is gitignored and must never be committed.
 * ===================================================================== */
#pragma once

// WiFi fallback network (used only when no Ethernet link is detected).
#define WIFI_FALLBACK_SSID  "your-wifi-ssid"
#define WIFI_FALLBACK_PASS  "your-wifi-password"

// Over-the-air update password. Use a strong, unique value.
// Keep this in sync with upload_flags (--auth=...) in platformio.ini.
#define OTA_PASSWORD        "change-me-to-a-strong-password"
