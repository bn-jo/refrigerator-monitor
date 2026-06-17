/* =====================================================================
 *  Settings.h — persistent, JSON-backed configuration (LittleFS)
 * ===================================================================== */
#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

struct FridgeCfg {
  String name;         // human-readable name, e.g. "Milk Refrigerator"
  float  threshold;    // alarm threshold (°C); alarm when temp > threshold
  bool   enabled;      // sensor in service
};

struct EmailCfg {
  String  host;        // SMTP server
  uint16_t port;       // SMTP port (465 SSL / 587 STARTTLS)
  String  user;        // SMTP username
  String  pass;        // SMTP password
  String  sender;      // From: address
  String  recipient;   // To: address
  bool    enabled;
};

struct UserCred {
  String username;
  String salt;         // random per-user salt
  String pwHashHex;    // SHA-256( salt + password ), hex
};

class SettingsManager {
public:
  FridgeCfg fridge[NUM_FRIDGES];
  String    outdoorName;
  EmailCfg  email;
  std::vector<UserCred> users;   // multiple login accounts
  String    timezone;       // POSIX TZ string
  String    deviceName;

  void  begin();            // load from flash or seed defaults
  bool  load();
  bool  save();             // atomic write (temp file + rename)
  void  applyDefaults();

  // ---- user management ----
  void  addOrUpdateUser(const String& user, const String& plain);
  bool  removeUser(const String& user);
  bool  userExists(const String& user) const;
  bool  verifyUser(const String& user, const String& plain) const;

  // Serialise the *public* (non-secret) settings for the web UI.
  String toPublicJson() const;
  // Merge an incoming settings JSON payload; returns false on parse error.
  bool   updateFromJson(const String& body, String& errOut);
};

extern SettingsManager Settings;
