/* =====================================================================
 *  main.cpp — Industrial Refrigerator Monitor
 *  Target: WT32-ETH01 (ESP32 + LAN8720 Ethernet)
 *
 *  Architecture:
 *    - setup() performs a safe, ordered bring-up of every subsystem.
 *    - A dedicated FreeRTOS task ("acq") samples the sensors once per
 *      minute, persists to the circular DB and runs alarm logic, so SMTP
 *      / flash latency never blocks networking.
 *    - loop() services the network FSM, NTP, watchdogs and the deferred
 *      restart request. The web server itself is fully asynchronous.
 * ===================================================================== */
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "Logger.h"
#include "TimeSync.h"
#include "Settings.h"
#include "Network.h"
#include "Sensors.h"
#include "Storage.h"
#include "Alarms.h"
#include "Email.h"
#include "Auth.h"
#include "WebServer.h"
#include "Watchdog.h"

static TaskHandle_t acqTask = nullptr;

// ---------------------------------------------------------------------
//  OTA (over-the-air) update support
// ---------------------------------------------------------------------
#if OTA_ENABLED
static void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    const char* what = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    LOGW("ota", String("update started (") + what + ") — suspending acquisition");
    if (acqTask) vTaskSuspend(acqTask);   // avoid flash write contention
  });
  ArduinoOTA.onProgress([](unsigned int prog, unsigned int total) {
    Watchdog.feed();                       // keep the watchdog happy during write
  });
  ArduinoOTA.onEnd([]() {
    LOGI("ota", "update complete — rebooting");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    LOGE("ota", "update failed, error " + String((int)e));
    if (acqTask) vTaskResume(acqTask);
  });
  ArduinoOTA.begin();
  LOGI("ota", "OTA ready (hostname '" DEVICE_HOSTNAME "')");
}
#endif

// ---------------------------------------------------------------------
//  Acquisition task — one sample per minute (runs on core 1)
// ---------------------------------------------------------------------
static void acquisitionTask(void*) {
  // Prime first reading shortly after boot instead of waiting a full minute.
  uint32_t nextSample = millis() + 3000;
  for (;;) {
    if ((int32_t)(millis() - nextSample) >= 0) {
      nextSample += SENSOR_SAMPLE_MS;

      Sensors.requestAll();
      while (!Sensors.conversionReady()) vTaskDelay(pdMS_TO_TICKS(50));
      Sensors.readAll();

      float temps[NUM_FRIDGES];
      bool  valid[NUM_FRIDGES];
      bool  anyValid = false;
      for (int i = 0; i < NUM_FRIDGES; i++) {
        valid[i] = (Sensors.readings[i].state == SENSOR_OK);
        temps[i] = valid[i] ? Sensors.readings[i].tempC : NAN;
        if (valid[i]) anyValid = true;
      }

      // Only persist once the clock is valid, so timestamps are meaningful.
      if (TimeSync.isSynced()) {
        bool ok = Storage.append(TimeSync.now(), temps);
        Web.dataAcqOk = ok && anyValid;
        Web.lastAcq   = TimeSync.now();
      }
      Alarms.evaluate(temps, valid);
      Watchdog.heartbeatSensors();
    }
    // E-mail the web address if the device landed on a brand-new IP. Runs here
    // (not in loop()) because SMTP blocks and shares the session with alarms.
    Net.serviceAddressNotice();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Refrigerator Monitor v" FW_VERSION " booting ===");

  // 1) Filesystem first — everything else depends on it.
  // NOTE: the data partition is labeled "littlefs" in partitions.csv, so the
  // label MUST be passed explicitly (the default LittleFS label is "spiffs").
  if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    Serial.println("FATAL: LittleFS mount failed even after format");
  }
  // Defensive: a corrupt superblock can "mount" with zero/bad geometry, which
  // later triggers a divide-by-zero inside lfs_alloc on the first write. If the
  // reported size is implausible, force a clean reformat before anything writes.
  if (LittleFS.totalBytes() == 0 || LittleFS.usedBytes() > LittleFS.totalBytes()) {
    Serial.println("WARN: LittleFS geometry invalid — reformatting");
    LittleFS.format();
    LittleFS.begin(true, "/littlefs", 10, "littlefs");
  }

  // 2) Logger + time (logger timestamps fall back to uptime until NTP).
  Log.begin();
  Settings.begin();
  TimeSync.begin(Settings.timezone);
  LOGI("system", "system boot — firmware v" FW_VERSION " (" FW_BUILD_DATE ")");

  // 3) Core subsystems.
  Storage.begin();
  Sensors.begin();
  Alarms.begin();
  Email.begin();
  Auth.begin();

  // 4) Network + web (web depends on auth/settings already being ready).
  Net.begin();
  Web.begin();

  // 5) Watchdog last, then spawn the acquisition task pinned to core 1.
  Watchdog.begin();
  xTaskCreatePinnedToCore(acquisitionTask, "acq", 8192, nullptr, 2, &acqTask, 1);

#if OTA_ENABLED
  setupOTA();
#endif

  LOGI("system", "boot complete");
}

void loop() {
  Net.loop();
#if OTA_ENABLED
  ArduinoOTA.handle();
#endif
  TimeSync.loop();
  Auth.sweep();
  Watchdog.feed();
  Watchdog.checkSoftware();

  // Deferred, safe restart (requested from the web UI / REST API).
  if (Web.restartRequested) {
    LOGW("system", "performing safe restart");
    Serial.flush();
    delay(500);          // allow the HTTP 200 response to flush to the client
    ESP.restart();
  }

  delay(50);
}
