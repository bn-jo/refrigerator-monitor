/* =====================================================================
 *  WebServer.h — async HTTP server: static UI + REST API + auth
 * ===================================================================== */
#pragma once
#include <Arduino.h>

class WebServerClass {
public:
  void begin();
  // Set by main loop so /api/status can report acquisition health.
  volatile bool   dataAcqOk = true;
  volatile time_t lastAcq   = 0;
  volatile bool   restartRequested = false;

private:
  void routes();
};

extern WebServerClass Web;
