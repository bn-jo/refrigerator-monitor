/* =====================================================================
 *  TimeSync.h — NTP synchronisation, timezone handling, timestamps
 * ===================================================================== */
#pragma once
#include <Arduino.h>

class TimeSyncClass {
public:
  void   begin(const String& tz);
  void   loop();                 // periodic re-sync (non-blocking)
  void   setTimezone(const String& tz);

  bool   isSynced() const { return _synced; }
  time_t now() const { return time(nullptr); }
  String isoNow();               // "2026-06-16 14:03:21" (local time)
  String isoFrom(time_t t);
  String uptimeStr();            // "3d 04:11:27"

private:
  bool     _synced = false;
  uint32_t _lastSync = 0;
  String   _tz;
};

extern TimeSyncClass TimeSync;
