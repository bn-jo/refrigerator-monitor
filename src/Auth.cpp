/* =====================================================================
 *  Auth.cpp
 * ===================================================================== */
#include "Auth.h"
#include "Settings.h"
#include "Logger.h"

AuthClass Auth;

void AuthClass::begin() {
  for (auto& s : _sessions) { s.active = false; s.token = ""; s.lastSeen = 0; }
}

String AuthClass::newToken() {
  String t;
  for (int i = 0; i < 16; i++) { char b[3]; sprintf(b, "%02x", (uint8_t)esp_random()); t += b; }
  return t;
}

bool AuthClass::login(const String& user, const String& pass, String& tokenOut) {
  if (!Settings.verifyUser(user, pass)) {
    LOGW("auth", "failed login attempt for user '" + user + "'");
    return false;
  }
  // Find a free slot, or evict the oldest.
  int slot = -1; uint32_t oldest = UINT32_MAX;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!_sessions[i].active) { slot = i; break; }
    if (_sessions[i].lastSeen < oldest) { oldest = _sessions[i].lastSeen; slot = i; }
  }
  _sessions[slot] = { newToken(), millis(), true };
  tokenOut = _sessions[slot].token;
  LOGI("auth", "user '" + user + "' logged in");
  return true;
}

void AuthClass::logout(const String& token) {
  for (auto& s : _sessions)
    if (s.active && s.token == token) { s.active = false; s.token = ""; }
}

bool AuthClass::validate(const String& token) {
  if (token.isEmpty()) return false;
  for (auto& s : _sessions) {
    if (s.active && s.token == token) {
      if (millis() - s.lastSeen > SESSION_TIMEOUT_MS) { s.active = false; return false; }
      s.lastSeen = millis();   // sliding expiry
      return true;
    }
  }
  return false;
}

void AuthClass::sweep() {
  for (auto& s : _sessions)
    if (s.active && millis() - s.lastSeen > SESSION_TIMEOUT_MS) s.active = false;
}
