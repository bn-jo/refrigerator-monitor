/* =====================================================================
 *  config.h — compile-time configuration & global constants
 *  Industrial Refrigerator Monitor (WT32-ETH01 / ESP32-ETH01)
 * ===================================================================== */
#pragma once
#include <Arduino.h>

// --------------------------------------------------------------------
//  Secrets (WiFi fallback credentials, OTA password)
//  These are kept out of version control. Copy src/secrets.example.h
//  to src/secrets.h and fill in your own values before building.
// --------------------------------------------------------------------
#include "secrets.h"

// --------------------------------------------------------------------
//  Firmware identity
// --------------------------------------------------------------------
#define FW_VERSION        "1.0.0"
#define FW_BUILD_DATE     __DATE__ " " __TIME__
#define DEVICE_HOSTNAME   "fridge-monitor"

// --------------------------------------------------------------------
//  Sensor configuration
// --------------------------------------------------------------------
#define NUM_FRIDGES        5      // refrigerator sensors (history-logged)
#define NUM_SENSORS        6      // fridges + 1 outdoor
#define IDX_OUTDOOR        5      // index of outdoor sensor in arrays

// OneWire data pins (see docs/WIRING.md for strapping-pin rationale).
// GPIO4 and GPIO14 are free, non-strapping pins on the WT32-ETH01.
#define PIN_ONEWIRE_FRIDGE 4     // bus shared by the 5 refrigerator sensors
#define PIN_ONEWIRE_OUTDOOR 14   // dedicated bus for the outdoor sensor

#define DS18B20_RESOLUTION 12    // bits (12 = 0.0625 °C, ~750 ms conversion)
#define SENSOR_SAMPLE_MS   60000UL   // 1 sample per minute
#define SENSOR_RETRY_MAX   3         // re-reads before flagging a sensor fault

// Plausibility window for a valid DS18B20 reading (°C).
#define TEMP_MIN_VALID     -55.0f
#define TEMP_MAX_VALID      125.0f
#define DS18B20_ERR_VALUE  -127.0f   // DallasTemperature DEVICE_DISCONNECTED_C

// --------------------------------------------------------------------
//  Ethernet (WT32-ETH01 / LAN8720) — also mirrored in platformio.ini
// --------------------------------------------------------------------
#ifndef ETH_PHY_ADDR
  #define ETH_PHY_TYPE   ETH_PHY_LAN8720
  #define ETH_PHY_ADDR   1
  #define ETH_PHY_MDC    23
  #define ETH_PHY_MDIO   18
  #define ETH_PHY_POWER  16
  #define ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN
#endif

// --------------------------------------------------------------------
//  Static IP (applied to both Ethernet and the WiFi fallback)
//  Set USE_STATIC_IP to 0 to use DHCP. Edit the octets to match your LAN.
//  NOTE: the IP must be inside your router's subnet and ideally OUTSIDE its
//  DHCP pool to avoid address conflicts.
// --------------------------------------------------------------------
#define USE_STATIC_IP        1
#define STATIC_IP_OCTETS     192,168,1,50      // device address
#define STATIC_GW_OCTETS     192,168,1,1       // router / gateway
#define STATIC_MASK_OCTETS   255,255,255,0     // subnet mask (/24)
#define STATIC_DNS1_OCTETS   192,168,1,1       // primary DNS (often = gateway)
#define STATIC_DNS2_OCTETS   8,8,8,8           // secondary DNS

// --------------------------------------------------------------------
//  WiFi fallback (used only when no Ethernet link is detected)
//  WIFI_FALLBACK_SSID / WIFI_FALLBACK_PASS are defined in src/secrets.h.
// --------------------------------------------------------------------
#define ETH_LINK_TIMEOUT_MS 15000UL  // wait this long for DHCP before WiFi

// --------------------------------------------------------------------
//  Time / NTP
// --------------------------------------------------------------------
#define NTP_SERVER_1   "pool.ntp.org"
#define NTP_SERVER_2   "time.google.com"
#define NTP_SERVER_3   "time.cloudflare.com"
// Default POSIX TZ string (configurable in Settings). Example = Israel.
#define DEFAULT_TZ     "IST-2IDT,M3.4.4/26,M10.5.0"
#define NTP_RESYNC_MS  3600000UL     // re-sync hourly

// Internet reachability probe (TCP connect) — detects "Internet lost".
#define NET_PROBE_HOST "1.1.1.1"
#define NET_PROBE_PORT 53
#define NET_PROBE_MS   30000UL

// --------------------------------------------------------------------
//  Storage (LittleFS circular history database)
// --------------------------------------------------------------------
#define HISTORY_FILE      "/history.db"
#define HISTORY_DAYS      30
#define HISTORY_CAPACITY  (HISTORY_DAYS * 24 * 60)   // 43200 records
#define SETTINGS_FILE     "/settings.json"
#define LOG_FILE          "/events.log"
// Log cap is sized so history is never starved. Budget on the 896 KB
// LittleFS partition: history ~648 KB + logs (2×48 KB) + web ~40 KB
// + settings ≈ 784 KB, leaving headroom for filesystem overhead.
#define LOG_MAX_BYTES     49152UL     // 48 KB, rotated to events.log.1
#define LOG_MAX_LINES_API 500         // max lines returned/held for the UI

// --------------------------------------------------------------------
//  Alarm / e-mail timing
// --------------------------------------------------------------------
#define ALARM_HYSTERESIS_C   0.5f     // °C below threshold to clear an alarm
#define ALARM_NEAR_MARGIN_C  1.0f     // °C below threshold → "yellow" warning
#define ALARM_REMINDER_MS    1800000UL // 30 min reminder cadence
#define ALARM_CONFIRM_COUNT  2         // consecutive readings before alarming

// --------------------------------------------------------------------
//  Security / sessions
// --------------------------------------------------------------------
#define SESSION_TIMEOUT_MS   1800000UL // 30 min idle session timeout
#define SESSION_COOKIE       "FMSESSION"
#define MAX_SESSIONS         4
#define DEFAULT_USER         "admin"
#define DEFAULT_PASS         "admin"   // forced change is recommended on login

// --------------------------------------------------------------------
//  Watchdog
// --------------------------------------------------------------------
#define WDT_TIMEOUT_S        30        // task watchdog timeout (seconds)

// --------------------------------------------------------------------
//  OTA (over-the-air) firmware/filesystem updates
//  CHANGE the password before deployment. Uploads use:
//    pio run -e ota -t upload       (firmware)
//    pio run -e ota -t uploadfs     (web UI)
// --------------------------------------------------------------------
#define OTA_ENABLED   1
// OTA_PASSWORD is defined in src/secrets.h.
