/* =====================================================================
 *  Watchdog.h — hardware (TWDT) + software watchdog supervision
 * ===================================================================== */
#pragma once
#include <Arduino.h>

class WatchdogClass {
public:
  void begin();                 // init Task WDT, subscribe current task
  void feed();                  // pet the hardware watchdog (call in loop)

  // Software watchdog: any subsystem updates its heartbeat; if a tracked
  // heartbeat goes stale the device performs a clean, logged restart.
  void heartbeatSensors() { _hbSensors = millis(); }
  void checkSoftware();

private:
  uint32_t _hbSensors = 0;
};

extern WatchdogClass Watchdog;
