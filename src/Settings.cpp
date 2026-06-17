/* =====================================================================
 *  Settings.cpp
 * ===================================================================== */
#include "Settings.h"
#include "Logger.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>

SettingsManager Settings;

// ---- small crypto helpers -------------------------------------------
static String sha256Hex(const String& in) {
  uint8_t out[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 (not 224)
  mbedtls_sha256_update(&ctx, (const uint8_t*)in.c_str(), in.length());
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
  char hex[65];
  for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", out[i]);
  hex[64] = 0;
  return String(hex);
}

static String randomHex(size_t bytes) {
  String s;
  for (size_t i = 0; i < bytes; i++) {
    char b[3];
    sprintf(b, "%02x", (uint8_t)esp_random());
    s += b;
  }
  return s;
}

// ---------------------------------------------------------------------
void SettingsManager::applyDefaults() {
  const char* names[NUM_FRIDGES] = {
    "Refrigerator 1", "Refrigerator 2", "Refrigerator 3",
    "Refrigerator 4", "Refrigerator 5"
  };
  const float th[NUM_FRIDGES] = { 4, 6, 5, 3, 4 };
  for (int i = 0; i < NUM_FRIDGES; i++) {
    fridge[i].name      = names[i];
    fridge[i].threshold = th[i];
    fridge[i].enabled   = true;
  }
  outdoorName = "Outdoor";
  email = { "", 465, "", "", "", "", false };
  timezone   = DEFAULT_TZ;
  deviceName = DEVICE_HOSTNAME;
  wifiSsid   = WIFI_FALLBACK_SSID;
  wifiPass   = WIFI_FALLBACK_PASS;

  users.clear();
  addOrUpdateUser(DEFAULT_USER, DEFAULT_PASS);
}

void SettingsManager::addOrUpdateUser(const String& user, const String& plain) {
  for (auto& u : users) {
    if (u.username == user) {                 // update existing user's password
      if (u.salt.isEmpty()) u.salt = randomHex(16);
      u.pwHashHex = sha256Hex(u.salt + plain);
      return;
    }
  }
  UserCred c;                                  // new user
  c.username  = user;
  c.salt      = randomHex(16);
  c.pwHashHex = sha256Hex(c.salt + plain);
  users.push_back(c);
}

bool SettingsManager::removeUser(const String& user) {
  if (users.size() <= 1) return false;         // never delete the last account
  for (size_t i = 0; i < users.size(); i++) {
    if (users[i].username == user) { users.erase(users.begin() + i); return true; }
  }
  return false;
}

bool SettingsManager::userExists(const String& user) const {
  for (auto& u : users) if (u.username == user) return true;
  return false;
}

bool SettingsManager::verifyUser(const String& user, const String& plain) const {
  for (auto& u : users)
    if (u.username == user) return sha256Hex(u.salt + plain) == u.pwHashHex;
  return false;
}

// ---------------------------------------------------------------------
void SettingsManager::begin() {
  if (!load()) {
    LOGW("settings", "no valid settings file — seeding defaults");
    applyDefaults();
    save();
  }
}

bool SettingsManager::load() {
  // Settings live in NVS (a separate flash region) so they survive BOTH a
  // filesystem re-upload (uploadfs) and a firmware OTA. Fall back to the
  // legacy LittleFS file once, to migrate older installs.
  Preferences p;
  p.begin("fridgemon", true);
  String raw = p.getString("cfg", "");
  p.end();
  if (raw.isEmpty()) {
    File f = LittleFS.open(SETTINGS_FILE, "r");
    if (f) { raw = f.readString(); f.close(); }
  }
  if (raw.isEmpty()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, raw)) return false;

  JsonArray fr = doc["fridges"];
  if (fr.size() != NUM_FRIDGES) return false;
  for (int i = 0; i < NUM_FRIDGES; i++) {
    String defName = "Refrigerator " + String(i + 1);
    fridge[i].name      = fr[i]["name"]   | defName;
    fridge[i].threshold = fr[i]["thr"]    | 4.0f;
    fridge[i].enabled   = fr[i]["en"]     | true;
  }
  outdoorName     = doc["outdoorName"] | "Outdoor";
  timezone        = doc["tz"]          | DEFAULT_TZ;
  deviceName      = doc["deviceName"]  | DEVICE_HOSTNAME;
  wifiSsid        = doc["wifi"]["ssid"] | String(WIFI_FALLBACK_SSID);
  wifiPass        = doc["wifi"]["pass"] | String(WIFI_FALLBACK_PASS);

  email.host      = doc["email"]["host"]      | "";
  email.port      = doc["email"]["port"]      | 465;
  email.user      = doc["email"]["user"]      | "";
  email.pass      = doc["email"]["pass"]      | "";
  email.sender    = doc["email"]["sender"]    | "";
  email.recipient = doc["email"]["recipient"] | "";
  email.enabled   = doc["email"]["enabled"]   | false;

  // Users: new multi-user array, with migration from the old single "auth".
  users.clear();
  if (doc["users"].is<JsonArray>()) {
    for (JsonObject u : doc["users"].as<JsonArray>()) {
      UserCred c;
      c.username  = u["user"] | "";
      c.salt      = u["salt"] | "";
      c.pwHashHex = u["hash"] | "";
      if (c.username.length() && c.salt.length() && c.pwHashHex.length())
        users.push_back(c);
    }
  } else if (doc["auth"]["user"].is<const char*>()) {   // legacy migration
    UserCred c;
    c.username  = doc["auth"]["user"] | DEFAULT_USER;
    c.salt      = doc["auth"]["salt"] | "";
    c.pwHashHex = doc["auth"]["hash"] | "";
    if (c.salt.length() && c.pwHashHex.length()) users.push_back(c);
  }
  if (users.empty()) addOrUpdateUser(DEFAULT_USER, DEFAULT_PASS);  // never lock out
  return true;
}

bool SettingsManager::save() {
  JsonDocument doc;
  JsonArray fr = doc["fridges"].to<JsonArray>();
  for (int i = 0; i < NUM_FRIDGES; i++) {
    JsonObject o = fr.add<JsonObject>();
    o["name"] = fridge[i].name;
    o["thr"]  = fridge[i].threshold;
    o["en"]   = fridge[i].enabled;
  }
  doc["outdoorName"] = outdoorName;
  doc["tz"]          = timezone;
  doc["deviceName"]  = deviceName;
  doc["wifi"]["ssid"] = wifiSsid;
  doc["wifi"]["pass"] = wifiPass;
  doc["email"]["host"]      = email.host;
  doc["email"]["port"]      = email.port;
  doc["email"]["user"]      = email.user;
  doc["email"]["pass"]      = email.pass;
  doc["email"]["sender"]    = email.sender;
  doc["email"]["recipient"] = email.recipient;
  doc["email"]["enabled"]   = email.enabled;
  JsonArray us = doc["users"].to<JsonArray>();
  for (auto& u : users) {
    JsonObject o = us.add<JsonObject>();
    o["user"] = u.username;
    o["salt"] = u.salt;
    o["hash"] = u.pwHashHex;
  }

  // Persist to NVS (survives uploadfs and firmware OTA).
  String out;
  serializeJson(doc, out);
  Preferences p;
  p.begin("fridgemon", false);
  size_t n = p.putString("cfg", out);
  p.end();
  if (n == 0) { LOGE("settings", "NVS write failed"); return false; }
  return true;
}

String SettingsManager::toPublicJson() const {
  JsonDocument doc;
  JsonArray fr = doc["fridges"].to<JsonArray>();
  for (int i = 0; i < NUM_FRIDGES; i++) {
    JsonObject o = fr.add<JsonObject>();
    o["name"] = fridge[i].name;
    o["thr"]  = fridge[i].threshold;
    o["en"]   = fridge[i].enabled;
  }
  doc["outdoorName"] = outdoorName;
  doc["tz"]          = timezone;
  doc["deviceName"]  = deviceName;
  // WiFi fallback: expose the SSID but never the password.
  doc["wifi"]["ssid"]    = wifiSsid;
  doc["wifi"]["hasPass"] = !wifiPass.isEmpty();
  // E-mail: expose everything except the password.
  doc["email"]["host"]      = email.host;
  doc["email"]["port"]      = email.port;
  doc["email"]["user"]      = email.user;
  doc["email"]["sender"]    = email.sender;
  doc["email"]["recipient"] = email.recipient;
  doc["email"]["enabled"]   = email.enabled;
  doc["email"]["hasPass"]   = !email.pass.isEmpty();
  // Expose the list of usernames only (never salts/hashes).
  JsonArray us = doc["users"].to<JsonArray>();
  for (auto& u : users) us.add(u.username);
  String out;
  serializeJson(doc, out);
  return out;
}

bool SettingsManager::updateFromJson(const String& body, String& errOut) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { errOut = "invalid JSON"; return false; }

  if (doc["fridges"].is<JsonArray>()) {
    JsonArray fr = doc["fridges"];
    for (int i = 0; i < NUM_FRIDGES && i < (int)fr.size(); i++) {
      if (fr[i]["name"].is<const char*>()) fridge[i].name = fr[i]["name"].as<String>();
      if (fr[i]["thr"].is<float>() || fr[i]["thr"].is<int>())
        fridge[i].threshold = fr[i]["thr"].as<float>();
      if (fr[i]["en"].is<bool>()) fridge[i].enabled = fr[i]["en"].as<bool>();
    }
  }
  if (doc["outdoorName"].is<const char*>()) outdoorName = doc["outdoorName"].as<String>();
  if (doc["deviceName"].is<const char*>())  deviceName  = doc["deviceName"].as<String>();
  if (doc["tz"].is<const char*>())          timezone    = doc["tz"].as<String>();

  if (doc["wifi"].is<JsonObject>()) {
    JsonObject w = doc["wifi"];
    if (w["ssid"].is<const char*>()) wifiSsid = w["ssid"].as<String>();
    // Only overwrite the password if a non-empty one was supplied.
    if (w["pass"].is<const char*>() && strlen(w["pass"]) > 0)
      wifiPass = w["pass"].as<String>();
  }

  if (doc["email"].is<JsonObject>()) {
    JsonObject e = doc["email"];
    if (e["host"].is<const char*>())      email.host      = e["host"].as<String>();
    if (e["port"].is<int>())              email.port      = e["port"].as<uint16_t>();
    if (e["user"].is<const char*>())      email.user      = e["user"].as<String>();
    if (e["sender"].is<const char*>())    email.sender    = e["sender"].as<String>();
    if (e["recipient"].is<const char*>()) email.recipient = e["recipient"].as<String>();
    if (e["enabled"].is<bool>())          email.enabled   = e["enabled"].as<bool>();
    // Only overwrite the password if a non-empty one was supplied.
    if (e["pass"].is<const char*>() && strlen(e["pass"]) > 0)
      email.pass = e["pass"].as<String>();
  }
  // ---- user management operations ----
  // Add or update a user: {"addUser":{"user":"yosi","pass":"yosi"}}
  if (doc["addUser"].is<JsonObject>()) {
    String u = doc["addUser"]["user"] | "";
    String p = doc["addUser"]["pass"] | "";
    if (u.isEmpty() || p.isEmpty()) { errOut = "user and pass required"; return false; }
    addOrUpdateUser(u, p);
  }
  // Remove a user: {"delUser":"name"} (refuses to delete the last account).
  if (doc["delUser"].is<const char*>()) {
    if (!removeUser(doc["delUser"].as<String>())) {
      errOut = "cannot remove (last account or not found)"; return false;
    }
  }
  // Optional password change for an existing user:
  // {"newPassword":"...", "passwordUser":"admin"}  (defaults to first user)
  if (doc["newPassword"].is<const char*>() && strlen(doc["newPassword"]) >= 4) {
    String who = doc["passwordUser"] | (users.empty() ? String(DEFAULT_USER) : users[0].username);
    addOrUpdateUser(who, doc["newPassword"].as<String>());
  }

  if (!save()) { errOut = "flash write failed"; return false; }
  return true;
}
