/* =====================================================================
 *  Alarms.h — threshold evaluation, hysteresis, anti-spam e-mail logic
 * ===================================================================== */
#pragma once
#include <Arduino.h>
#include "config.h"

enum AlarmStatus { ST_NORMAL, ST_NEAR, ST_ALARM };

struct FridgeAlarm {
  AlarmStatus status = ST_NORMAL;
  bool        active = false;     // confirmed alarm latched
  uint8_t     confirm = 0;        // consecutive over-threshold readings
  uint32_t    lastEmailMs = 0;    // last alarm/reminder e-mail
};

class AlarmsClass {
public:
  void begin();
  // Call once per acquisition cycle with the latest fridge temperatures
  // and their validity. Drives e-mail notifications.
  void evaluate(const float temps[NUM_FRIDGES], const bool valid[NUM_FRIDGES]);

  AlarmStatus statusOf(int i) const { return _a[i].status; }
  bool        anyActive() const;

private:
  FridgeAlarm _a[NUM_FRIDGES];
};

extern AlarmsClass Alarms;
