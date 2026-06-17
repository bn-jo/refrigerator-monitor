/* =====================================================================
 *  Logger.h — event logging to LittleFS with rotation + serial mirror
 * ===================================================================== */
#pragma once
#include <Arduino.h>

enum LogLevel { LOG_INFO = 0, LOG_WARN = 1, LOG_ERROR = 2 };

class EventLogger {
public:
  void begin();
  // category = short tag (e.g. "net", "sensor"); msg = free text.
  void log(LogLevel level, const char* category, const String& msg);

  String tail(size_t maxLines, const String& levelFilter,
              const String& catFilter);   // JSON array for the UI
  bool   clear();
  size_t fileSize();

private:
  void rotateIfNeeded();
  SemaphoreHandle_t _mtx = nullptr;
};

extern EventLogger Log;

// Convenience macros
#define LOGI(cat, msg) Log.log(LOG_INFO,  cat, msg)
#define LOGW(cat, msg) Log.log(LOG_WARN,  cat, msg)
#define LOGE(cat, msg) Log.log(LOG_ERROR, cat, msg)
