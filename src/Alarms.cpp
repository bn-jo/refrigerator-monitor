/* =====================================================================
 *  Alarms.cpp
 * ===================================================================== */
#include "Alarms.h"
#include "Settings.h"
#include "Email.h"
#include "Logger.h"

AlarmsClass Alarms;

void AlarmsClass::begin() {
  for (auto& a : _a) a = FridgeAlarm{};
}

bool AlarmsClass::anyActive() const {
  for (auto& a : _a) if (a.active) return true;
  return false;
}

void AlarmsClass::evaluate(const float temps[NUM_FRIDGES],
                           const bool valid[NUM_FRIDGES]) {
  for (int i = 0; i < NUM_FRIDGES; i++) {
    FridgeAlarm& a = _a[i];
    if (!Settings.fridge[i].enabled) { a.status = ST_NORMAL; a.active = false; continue; }
    if (!valid[i]) continue;   // ignore faulted readings for alarm logic

    float t   = temps[i];
    float thr = Settings.fridge[i].threshold;
    const String& name = Settings.fridge[i].name;

    // --- visual status (dashboard color) ---
    if (t > thr)                              a.status = ST_ALARM;
    else if (t >= thr - ALARM_NEAR_MARGIN_C)  a.status = ST_NEAR;
    else                                      a.status = ST_NORMAL;

    // --- latched alarm with confirmation + hysteresis ---
    if (!a.active) {
      if (t > thr) {
        if (++a.confirm >= ALARM_CONFIRM_COUNT) {
          a.active = true;
          a.lastEmailMs = millis();
          LOGE("alarm", name + " ALARM: " + String(t, 1) + "C > " +
               String(thr, 1) + "C");
          Email.sendAlarm(name, t, thr, false);   // first alarm: immediate
        }
      } else {
        a.confirm = 0;
      }
    } else {
      // Alarm is active: clear only once below threshold − hysteresis.
      if (t <= thr - ALARM_HYSTERESIS_C) {
        a.active = false;
        a.confirm = 0;
        LOGI("alarm", name + " cleared: " + String(t, 1) + "C");
        Email.sendRecovery(name, t, thr);
      } else if (millis() - a.lastEmailMs >= ALARM_REMINDER_MS) {
        a.lastEmailMs = millis();
        LOGW("alarm", name + " still in alarm — sending reminder");
        Email.sendAlarm(name, t, thr, true);       // 30-min reminder
      }
    }
  }
}
