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
  void   maybePromoteStatic();      // adopt static IP on Ethernet if valid here
  void   maybePromoteStaticWifi();  // ditto for the WiFi fallback
  void   applyStaticEth();          // force static IP (DHCP-less network)
  void   startMdns();               // advertise <hostname>.local + http service

  bool   _ethUp = false;
  bool   _wifi  = false;
  bool   _inet  = false;
  uint32_t _lastProbe = 0;
  uint32_t _ethStart  = 0;
  bool     _fallbackTried   = false;
  bool     _netSettled      = false;  // Ethernet addressing decided for this link
  bool     _directStaticTried = false;// applied static after DHCP timed out
  bool     _wifiSettled     = false;  // WiFi addressing decided
  bool     _mdnsStarted     = false;
};

extern NetworkClass Net;
