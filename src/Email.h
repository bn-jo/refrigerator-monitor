/* =====================================================================
 *  Email.h — SMTP alarm / recovery notifications (ESP-Mail-Client)
 * ===================================================================== */
#pragma once
#include <Arduino.h>

class EmailClass {
public:
  // Queue a message; actual sending happens on a dedicated task so the
  // monitoring loop is never blocked by SMTP latency.
  void begin();
  bool sendAlarm(const String& fridgeName, float temp, float threshold,
                 bool reminder);
  bool sendRecovery(const String& fridgeName, float temp, float threshold);
  bool sendTest();

private:
  bool send(const String& subject, const String& htmlBody);
};

extern EmailClass Email;
