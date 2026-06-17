/* =====================================================================
 *  Auth.h — session-based authentication (hashed password + cookies)
 * ===================================================================== */
#pragma once
#include <Arduino.h>
#include "config.h"

struct Session { String token; uint32_t lastSeen; bool active; };

class AuthClass {
public:
  void   begin();
  // Validate credentials; on success returns a fresh session token.
  bool   login(const String& user, const String& pass, String& tokenOut);
  void   logout(const String& token);
  bool   validate(const String& token);   // checks existence + timeout
  void   sweep();                          // expire idle sessions

private:
  Session _sessions[MAX_SESSIONS];
  String  newToken();
};

extern AuthClass Auth;
