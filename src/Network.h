/* =====================================================================
 *  Network.h — Ethernet (LAN8720) with WiFi fallback + connectivity FSM
 * ===================================================================== */
#pragma once
#include <Arduino.h>

class NetworkClass {
public:
  void begin();
  void loop();                 // monitors link, drives reconnection

  bool   ethLinkUp()    const { return _ethUp; }
  bool   usingWifi()    const { return _wifi; }
  bool   internetUp()   const { return _inet; }
  String ip()           const;
  String mac()          const;
  String mode()         const { return _wifi ? "WiFi" : "Ethernet"; }
  int    rssi()         const;   // WiFi signal in dBm (0 when on Ethernet)

private:
  void   probeInternet();
  bool   _ethUp = false;
  bool   _wifi  = false;
  bool   _inet  = false;
  uint32_t _lastProbe = 0;
  uint32_t _ethStart  = 0;
  bool     _fallbackTried = false;
};

extern NetworkClass Net;
