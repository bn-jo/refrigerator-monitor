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
  // Notify the recipient that the device is reachable at a new web address
  // (sent only when an IP is seen that was never used before).
  bool sendAddress(const String& ip, const String& mode);

private:
  bool send(const String& subject, const String& htmlBody);
};

extern EmailClass Email;
