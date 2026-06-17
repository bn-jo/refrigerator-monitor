/* =====================================================================
 *  WebServer.cpp — ESPAsyncWebServer routes
 * ===================================================================== */
#include "WebServer.h"
#include "config.h"
#include "Settings.h"
#include "Sensors.h"
#include "Storage.h"
#include "Alarms.h"
#include "Network.h"
#include "TimeSync.h"
#include "Logger.h"
#include "Auth.h"
#include "Email.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "web_assets.h"   // auto-generated gzipped UI (scripts/embed_web.py)

WebServerClass Web;
static AsyncWebServer server(80);

// ---- helpers --------------------------------------------------------
static String cookieToken(AsyncWebServerRequest* r) {
  if (!r->hasHeader("Cookie")) return "";
  String c = r->header("Cookie");
  int i = c.indexOf(SESSION_COOKIE "=");
  if (i < 0) return "";
  i += strlen(SESSION_COOKIE) + 1;
  int e = c.indexOf(';', i);
  return (e < 0) ? c.substring(i) : c.substring(i, e);
}

static bool authed(AsyncWebServerRequest* r) {
  return Auth.validate(cookieToken(r));
}

// Reject unauthenticated API calls with 401.
static bool guard(AsyncWebServerRequest* r) {
  if (authed(r)) return true;
  r->send(401, "application/json", "{\"error\":\"unauthorized\"}");
  return false;
}

static const char* stateStr(SensorState s) {
  switch (s) { case SENSOR_OK: return "ok"; case SENSOR_FAULT: return "fault";
    case SENSOR_OUTOFRANGE: return "outofrange"; default: return "missing"; }
}
static const char* alarmStr(AlarmStatus s) {
  switch (s) { case ST_ALARM: return "alarm"; case ST_NEAR: return "near";
    default: return "normal"; }
}

// ---- JSON builders --------------------------------------------------
static String statusJson() {
  JsonDocument d;
  d["fw"]          = FW_VERSION;
  d["build"]       = FW_BUILD_DATE;
  d["device"]      = Settings.deviceName;
  d["mode"]        = Net.mode();
  d["ethUp"]       = Net.ethLinkUp();
  d["wifi"]        = Net.usingWifi();
  d["rssi"]        = Net.rssi();
  d["internet"]    = Net.internetUp();
  d["ip"]          = Net.ip();
  d["mac"]         = Net.mac();
  d["time"]        = TimeSync.isoNow();
  d["timeSynced"]  = TimeSync.isSynced();
  d["tz"]          = Settings.timezone;
  d["uptime"]      = TimeSync.uptimeStr();
  d["heapFree"]    = ESP.getFreeHeap();
  d["records"]     = Storage.recordCount();
  d["storageOk"]   = Storage.healthy();
  d["writeFails"]  = Storage.writeFailures();
  d["dataAcqOk"]   = Web.dataAcqOk;
  d["lastAcq"]     = (uint32_t)Web.lastAcq;
  d["alarmActive"] = Alarms.anyActive();
  String out; serializeJson(d, out); return out;
}

static String temperaturesJson() {
  JsonDocument d;
  JsonArray arr = d["sensors"].to<JsonArray>();
  for (int i = 0; i < NUM_SENSORS; i++) {
    JsonObject o = arr.add<JsonObject>();
    bool fridge = (i < NUM_FRIDGES);
    o["index"]   = i;
    o["name"]    = fridge ? Settings.fridge[i].name : Settings.outdoorName;
    o["outdoor"] = !fridge;
    SensorReading& s = Sensors.readings[i];
    if (s.state == SENSOR_OK) o["temp"] = round(s.tempC * 10) / 10.0;
    else                      o["temp"] = nullptr;
    o["sensor"]  = stateStr(s.state);
    o["romId"]   = s.romId;
    o["lastValid"] = (uint32_t)s.lastValid;
    if (fridge) {
      o["threshold"] = Settings.fridge[i].threshold;
      o["status"]    = alarmStr(Alarms.statusOf(i));
      o["enabled"]   = Settings.fridge[i].enabled;
    }
  }
  String out; serializeJson(d, out); return out;
}

// ---------------------------------------------------------------------
void WebServerClass::routes() {
  // ---- Authentication ----
  server.on("/api/login", HTTP_POST,
    [](AsyncWebServerRequest* r) {}, nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument d;
      if (deserializeJson(d, data, len)) { r->send(400); return; }
      String tok;
      if (Auth.login(d["user"] | "", d["pass"] | "", tok)) {
        AsyncWebServerResponse* res = r->beginResponse(200,
          "application/json", "{\"ok\":true}");
        res->addHeader("Set-Cookie", String(SESSION_COOKIE) + "=" + tok +
          "; Path=/; HttpOnly; SameSite=Strict; Max-Age=1800");
        r->send(res);
      } else {
        r->send(401, "application/json", "{\"error\":\"bad credentials\"}");
      }
    });

  server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest* r) {
    Auth.logout(cookieToken(r));
    AsyncWebServerResponse* res = r->beginResponse(200, "application/json", "{\"ok\":true}");
    res->addHeader("Set-Cookie", String(SESSION_COOKIE) + "=; Path=/; Max-Age=0");
    r->send(res);
  });

  server.on("/api/authcheck", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "application/json",
            String("{\"authed\":") + (authed(r) ? "true" : "false") + "}");
  });

  // ---- Read endpoints ----
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    r->send(200, "application/json", statusJson());
  });

  server.on("/api/temperatures", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    r->send(200, "application/json", temperaturesJson());
  });

  server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    uint32_t range = 86400;            // default 24 h
    if (r->hasParam("range")) {
      String s = r->getParam("range")->value();
      if (s == "1h")  range = 3600;
      else if (s == "24h") range = 86400;
      else if (s == "7d")  range = 604800;
      else if (s == "30d") range = 2592000;
    }
    uint16_t maxPts = (range <= 3600) ? 120 :
                      (range <= 86400) ? 288 :
                      (range <= 604800) ? 336 : 360;
    r->send(200, "application/json", Storage.queryJson(range, maxPts));
  });

  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    r->send(200, "application/json", Settings.toPublicJson());
  });

  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    String lvl = r->hasParam("level") ? r->getParam("level")->value() : "";
    String cat = r->hasParam("cat")   ? r->getParam("cat")->value()   : "";
    r->send(200, "application/json", Log.tail(LOG_MAX_LINES_API, lvl, cat));
  });

  // Plain-text log download.
  server.on("/api/logs/download", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    if (LittleFS.exists(LOG_FILE)) {
      AsyncWebServerResponse* res = r->beginResponse(LittleFS, LOG_FILE, "text/plain");
      res->addHeader("Content-Disposition", "attachment; filename=events.log");
      r->send(res);
    } else r->send(404);
  });

  // ---- Write endpoints ----
  server.on("/api/settings", HTTP_POST,
    [](AsyncWebServerRequest* r) {}, nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t, size_t) {
      if (!guard(r)) return;
      String body((const char*)data, len), err;
      if (Settings.updateFromJson(body, err)) {
        TimeSync.setTimezone(Settings.timezone);
        LOGI("config", "settings updated via web UI");
        r->send(200, "application/json", "{\"ok\":true}");
      } else {
        r->send(400, "application/json", "{\"error\":\"" + err + "\"}");
      }
    });

  server.on("/api/email/test", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    bool ok = Email.sendTest();
    r->send(ok ? 200 : 500, "application/json",
            String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });

  server.on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    Log.clear();
    LOGI("config", "logs cleared via web UI");
    r->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!guard(r)) return;
    LOGW("system", "restart requested via web UI");
    r->send(200, "application/json", "{\"ok\":true}");
    Web.restartRequested = true;     // handled safely in main loop
  });

  // ---- Static files (embedded gzipped PROGMEM assets) ----
  // index.html requires a session; login.html, css and js are public.
  server.onNotFound([](AsyncWebServerRequest* r) {
    String url = r->url();
    if (url == "/") url = "/index.html";

    const WebAsset* a = nullptr;
    for (int i = 0; i < WEB_ASSET_COUNT; i++)
      if (url == WEB_ASSETS[i].path) { a = &WEB_ASSETS[i]; break; }

    if (a) {
      if (url == "/index.html" && !authed(r)) { r->redirect("/login.html"); return; }
      AsyncWebServerResponse* res =
        r->beginResponse(200, a->mime, a->data, a->len);
      res->addHeader("Content-Encoding", "gzip");
      // no-store: never cache UI assets, so updates always take effect
      // immediately (the device serves them fast from flash anyway).
      res->addHeader("Cache-Control", "no-store, max-age=0");
      r->send(res);
      return;
    }
    if (url.startsWith("/api/")) r->send(404, "application/json", "{\"error\":\"not found\"}");
    else r->redirect("/");
  });
}

void WebServerClass::begin() {
  routes();
  server.begin();
  LOGI("web", "HTTP server started on port 80");
}
