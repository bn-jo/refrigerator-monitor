/* =====================================================================
 *  Watchdog.cpp
 * ===================================================================== */
#include "Watchdog.h"
#include "config.h"
#include "Logger.h"
#include <esp_task_wdt.h>

WatchdogClass Watchdog;

void WatchdogClass::begin() {
  // Hardware Task Watchdog: panics+resets the chip if not fed in time.
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0)
  esp_task_wdt_config_t cfg = { .timeout_ms = WDT_TIMEOUT_S * 1000,
                                .idle_core_mask = 0, .trigger_panic = true };
  esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);              // watch the main loop task
  _hbSensors = millis();
  LOGI("wdt", "task watchdog armed (" + String(WDT_TIMEOUT_S) + "s)");
}

void WatchdogClass::feed() {
  esp_task_wdt_reset();
}

void WatchdogClass::checkSoftware() {
  // If sensor acquisition has stalled for >3 cycles, something is wedged.
  if (millis() - _hbSensors > SENSOR_SAMPLE_MS * 3 + 5000) {
    LOGE("wdt", "software watchdog: sensor task stalled — restarting");
    Serial.flush();
    delay(200);
    ESP.restart();
  }
}
