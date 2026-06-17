/* =====================================================================
 *  TimeSync.cpp
 * ===================================================================== */
#include "TimeSync.h"
#include "config.h"
#include <time.h>

TimeSyncClass TimeSync;

void TimeSyncClass::begin(const String& tz) {
  _tz = tz;
  // configTzTime applies the POSIX TZ string and starts SNTP.
  configTzTime(_tz.c_str(), NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  _lastSync = millis();
}

void TimeSyncClass::setTimezone(const String& tz) {
  _tz = tz;
  setenv("TZ", _tz.c_str(), 1);
  tzset();
}

void TimeSyncClass::loop() {
  // Mark as synced once the clock has advanced past a sane epoch.
  if (!_synced && time(nullptr) > 1700000000) _synced = true;

  if (millis() - _lastSync > NTP_RESYNC_MS) {
    _lastSync = millis();
    configTzTime(_tz.c_str(), NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  }
}

String TimeSyncClass::isoFrom(time_t t) {
  struct tm tmv;
  localtime_r(&t, &tmv);
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
  return String(buf);
}

String TimeSyncClass::isoNow() {
  if (!isSynced()) {
    // Before NTP, fall back to monotonic uptime so logs are still ordered.
    return "[+" + String(millis() / 1000) + "s]";
  }
  return isoFrom(time(nullptr));
}

String TimeSyncClass::uptimeStr() {
  uint32_t s = millis() / 1000;
  uint32_t d = s / 86400; s %= 86400;
  uint32_t h = s / 3600;  s %= 3600;
  uint32_t m = s / 60;    s %= 60;
  char buf[32];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
           (unsigned long)d, (unsigned long)h, (unsigned long)m,
           (unsigned long)s);
  return String(buf);
}
