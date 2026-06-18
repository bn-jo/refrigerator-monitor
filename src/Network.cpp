/* =====================================================================
 *  Network.cpp
 * ===================================================================== */
#include "Network.h"
#include "config.h"
#include "Logger.h"
#include "Settings.h"
#include <ETH.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>

NetworkClass Net;

// True when address `a` and `b` share the same subnet under `mask`.
static bool sameSubnet(const IPAddress& a, const IPAddress& b, const IPAddress& mask) {
  for (int i = 0; i < 4; i++)
    if ((a[i] & mask[i]) != (b[i] & mask[i])) return false;
  return true;
}

// Parse the configured static address fields. Returns false if the IP,
// gateway or mask is malformed (DNS entries are optional).
static bool parseStatic(IPAddress& ip, IPAddress& gw, IPAddress& mask,
                        IPAddress& d1, IPAddress& d2) {
  d1 = IPAddress(); d2 = IPAddress();
  d1.fromString(Settings.netDns1);
  d2.fromString(Settings.netDns2);
  return ip.fromString(Settings.netIp) && gw.fromString(Settings.netGw) &&
         mask.fromString(Settings.netMask);
}

static volatile bool s_ethGotIp = false;
static volatile bool s_ethConnected = false;

// Arduino-ESP32 network events (works for both ETH and WiFi STA).
static void onNetEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_CONNECTED:
      s_ethConnected = true; LOGI("net", "Ethernet link up"); break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      s_ethGotIp = true;
      LOGI("net", "Ethernet got IP " + ETH.localIP().toString()); break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      s_ethConnected = false; s_ethGotIp = false;
      LOGW("net", "Ethernet link down"); break;
    case ARDUINO_EVENT_ETH_STOP:
      s_ethConnected = false; s_ethGotIp = false; break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      LOGI("net", "WiFi got IP " + WiFi.localIP().toString()); break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      LOGW("net", "WiFi disconnected"); break;
    default: break;
  }
}

void NetworkClass::begin() {
  WiFi.onEvent(onNetEvent);
  WiFi.setHostname(DEVICE_HOSTNAME);
  // Start Ethernet (pins come from build flags / config.h).
  ETH.setHostname(DEVICE_HOSTNAME);
  ETH.begin();
  // Always bring the link up with DHCP first so the device joins ANY network.
  // If a static IP is configured and turns out to be valid on this network,
  // maybePromoteStatic() adopts it once the DHCP lease reveals the subnet.
  _ethStart = millis();
  LOGI("net", Settings.netDhcp ? "waiting for Ethernet DHCP..."
                               : "Ethernet DHCP first (static IP preferred)...");
}

// Adopt the configured static IP on Ethernet — but only when it belongs to the
// network we just joined via DHCP. On a different LAN the static address would
// be unreachable, so we keep the DHCP lease instead. This is what lets the same
// firmware keep its static address at home yet still work on any other network.
void NetworkClass::maybePromoteStatic() {
  if (Settings.netDhcp) {
    LOGI("net", "Ethernet on DHCP IP " + ETH.localIP().toString());
    return;
  }
  IPAddress ip, gw, mask, d1, d2;
  if (!parseStatic(ip, gw, mask, d1, d2)) {
    LOGW("net", "invalid static IP settings — keeping DHCP address");
    return;
  }
  // _directStaticTried: we already applied static directly (DHCP-less net).
  if (_directStaticTried || sameSubnet(ip, ETH.localIP(), ETH.subnetMask())) {
    if (ETH.config(ip, gw, mask, d1, d2))
      LOGI("net", "Ethernet static IP " + ip.toString());
    else
      LOGE("net", "Ethernet static IP config failed — staying on DHCP");
  } else {
    LOGW("net", "static IP " + ip.toString() + " not on this network (" +
                 ETH.localIP().toString() + "/" + ETH.subnetMask().toString() +
                 ") — using DHCP address");
  }
}

// Force the static IP onto Ethernet without a DHCP lease — used only when the
// network has no DHCP server (DHCP never answered within ETH_DHCP_TIMEOUT_MS).
void NetworkClass::applyStaticEth() {
  IPAddress ip, gw, mask, d1, d2;
  if (!parseStatic(ip, gw, mask, d1, d2)) {
    LOGE("net", "no DHCP and invalid static IP settings");
    return;
  }
  if (ETH.config(ip, gw, mask, d1, d2))
    LOGI("net", "no DHCP server — applied static IP " + ip.toString());
  else
    LOGE("net", "static IP config failed");
}

// Same promotion logic for the WiFi fallback interface.
void NetworkClass::maybePromoteStaticWifi() {
  if (Settings.netDhcp) return;
  IPAddress ip, gw, mask, d1, d2;
  if (!parseStatic(ip, gw, mask, d1, d2)) return;
  if (sameSubnet(ip, WiFi.localIP(), WiFi.subnetMask())) {
    if (WiFi.config(ip, gw, mask, d1, d2))
      LOGI("net", "WiFi static IP " + ip.toString());
    else
      LOGE("net", "WiFi static IP config failed");
  } else {
    LOGW("net", "static IP not on WiFi network — using DHCP address");
  }
}

// Bring up the mDNS responder so the device is always reachable by name
// (<hostname>.local) regardless of which IP DHCP assigned on a new network.
void NetworkClass::startMdns() {
  // ArduinoOTA (if enabled) starts mDNS in setup(); begin() then returns false
  // but the responder is already up, so advertise the web service and stop
  // retrying regardless of the return value.
  bool fresh = MDNS.begin(DEVICE_HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  _mdnsStarted = true;
  LOGI("net", String("mDNS responder up: ") + DEVICE_HOSTNAME + ".local" +
              (fresh ? "" : " (shared with OTA)"));
}

void NetworkClass::loop() {
  _ethUp = s_ethGotIp;

  // Once Ethernet has an address, decide (once) whether to keep the DHCP
  // lease or promote to the configured static IP.
  if (_ethUp && !_netSettled) {
    maybePromoteStatic();
    _netSettled = true;
  }

  // DHCP-less network: the PHY link is up but no DHCP answer arrived in time.
  // If a static IP is configured, apply it directly so we still get on the LAN.
  if (!_ethUp && !_netSettled && !_directStaticTried && !Settings.netDhcp &&
      s_ethConnected && millis() - _ethStart > ETH_DHCP_TIMEOUT_MS) {
    _directStaticTried = true;
    applyStaticEth();              // GOT_IP follows and settles via the block above
  }

  // Fallback to WiFi once if Ethernet fails to obtain an IP in time.
  if (!_ethUp && !_wifi && !_fallbackTried &&
      millis() - _ethStart > ETH_LINK_TIMEOUT_MS) {
    _fallbackTried = true;
    LOGW("net", "no Ethernet — falling back to WiFi SSID '" + Settings.wifiSsid + "'");
    WiFi.mode(WIFI_STA);
    // Join via DHCP first; promote to static afterwards only if it fits (below).
    WiFi.begin(Settings.wifiSsid.c_str(), Settings.wifiPass.c_str());
  }

  // If Ethernet recovers while on WiFi, prefer Ethernet again.
  if (_ethUp && _wifi) {
    LOGI("net", "Ethernet restored — disabling WiFi");
    WiFi.disconnect(true);
    _wifi = false;
    _wifiSettled = false;
  }
  if (!_ethUp && _fallbackTried) {
    bool nowWifi = (WiFi.status() == WL_CONNECTED);
    if (nowWifi && !_wifiSettled) {   // just connected: decide WiFi addressing once
      maybePromoteStaticWifi();
      _wifiSettled = true;
    }
    _wifi = nowWifi;
  }

  // Start mDNS as soon as we have any working interface (idempotent guard).
  if ((_ethUp || _wifi) && !_mdnsStarted) startMdns();

  // Periodic Internet reachability probe.
  if (millis() - _lastProbe > NET_PROBE_MS) {
    _lastProbe = millis();
    probeInternet();
  }
}

void NetworkClass::probeInternet() {
  bool linkUp = _ethUp || _wifi;
  if (!linkUp) {
    if (_inet) LOGW("net", "Internet connectivity lost (no link)");
    _inet = false;
    return;
  }
  WiFiClient c;
  c.setTimeout(3000);
  bool ok = c.connect(NET_PROBE_HOST, NET_PROBE_PORT);
  c.stop();
  if (ok != _inet) {
    LOGW("net", ok ? "Internet connectivity restored" : "Internet connectivity lost");
  }
  _inet = ok;
}

String NetworkClass::ip() const {
  if (_wifi) return WiFi.localIP().toString();
  return ETH.localIP().toString();
}

String NetworkClass::mac() const {
  if (_wifi) return WiFi.macAddress();
  return ETH.macAddress();
}

int NetworkClass::rssi() const {
  return _wifi ? WiFi.RSSI() : 0;   // Ethernet has no RSSI
}
