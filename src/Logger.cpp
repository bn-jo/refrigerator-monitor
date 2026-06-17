/* =====================================================================
 *  Logger.cpp
 * ===================================================================== */
#include "Logger.h"
#include "config.h"
#include "TimeSync.h"
#include <LittleFS.h>

EventLogger Log;

static const char* levelStr(LogLevel l) {
  switch (l) { case LOG_WARN: return "WARN"; case LOG_ERROR: return "ERROR";
               default: return "INFO"; }
}

void EventLogger::begin() {
  _mtx = xSemaphoreCreateMutex();
}

void EventLogger::rotateIfNeeded() {
  File f = LittleFS.open(LOG_FILE, "r");
  size_t sz = f ? f.size() : 0;
  if (f) f.close();
  if (sz >= LOG_MAX_BYTES) {
    LittleFS.remove(LOG_FILE ".1");
    LittleFS.rename(LOG_FILE, LOG_FILE ".1");  // keep one previous generation
  }
}

void EventLogger::log(LogLevel level, const char* category, const String& msg) {
  String line = TimeSync.isoNow() + " [" + levelStr(level) + "] " +
                category + ": " + msg;
  Serial.println(line);     // always mirror to serial console

  if (_mtx) xSemaphoreTake(_mtx, portMAX_DELAY);
  rotateIfNeeded();
  File f = LittleFS.open(LOG_FILE, "a");
  if (f) { f.println(line); f.close(); }
  if (_mtx) xSemaphoreGive(_mtx);
}

// Return the last maxLines lines (newest last) as a JSON array, applying
// optional level/category filters. Reads both rotated generations.
String EventLogger::tail(size_t maxLines, const String& levelFilter,
                         const String& catFilter) {
  if (_mtx) xSemaphoreTake(_mtx, portMAX_DELAY);

  // Collect lines from .1 then current, keep a sliding window of maxLines.
  String window[LOG_MAX_LINES_API];
  size_t count = 0, head = 0;
  const char* files[2] = { LOG_FILE ".1", LOG_FILE };
  for (int fi = 0; fi < 2; fi++) {
    File f = LittleFS.open(files[fi], "r");
    if (!f) continue;
    while (f.available()) {
      String l = f.readStringUntil('\n');
      l.trim();
      if (l.isEmpty()) continue;
      if (levelFilter.length() && l.indexOf("[" + levelFilter + "]") < 0) continue;
      if (catFilter.length()   && l.indexOf("] " + catFilter + ":")  < 0) continue;
      window[head] = l;
      head = (head + 1) % maxLines;
      if (count < maxLines) count++;
    }
    f.close();
  }
  if (_mtx) xSemaphoreGive(_mtx);

  String out = "[";
  size_t start = (count < maxLines) ? 0 : head;
  for (size_t i = 0; i < count; i++) {
    String l = window[(start + i) % maxLines];
    l.replace("\\", "\\\\");
    l.replace("\"", "\\\"");
    if (i) out += ",";
    out += "\"" + l + "\"";
  }
  out += "]";
  return out;
}

bool EventLogger::clear() {
  if (_mtx) xSemaphoreTake(_mtx, portMAX_DELAY);
  LittleFS.remove(LOG_FILE);
  LittleFS.remove(LOG_FILE ".1");
  if (_mtx) xSemaphoreGive(_mtx);
  return true;
}

size_t EventLogger::fileSize() {
  File f = LittleFS.open(LOG_FILE, "r");
  size_t s = f ? f.size() : 0;
  if (f) f.close();
  return s;
}
