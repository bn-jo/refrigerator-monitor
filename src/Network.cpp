/* =====================================================================
 *  Network.cpp
 * ===================================================================== */
#include "Network.h"
#include "config.h"
#include "Logger.h"
#include <ETH.h>
#include <WiFi.h>
#include <WiFiClient.h>

NetworkClass Net;

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

#if USE_STATIC_IP
static const IPAddress S_IP  (STATIC_IP_OCTETS);
static const IPAddress S_GW  (STATIC_GW_OCTETS);
static const IPAddress S_MASK(STATIC_MASK_OCTETS);
static const IPAddress S_DNS1(STATIC_DNS1_OCTETS);
static const IPAddress S_DNS2(STATIC_DNS2_OCTETS);
#endif

void NetworkClass::begin() {
  WiFi.onEvent(onNetEvent);
  WiFi.setHostname(DEVICE_HOSTNAME);
  // Start Ethernet (pins come from build flags / config.h).
  ETH.setHostname(DEVICE_HOSTNAME);
  ETH.begin();
#if USE_STATIC_IP
  // Apply the fixed address to the Ethernet interface.
  if (!ETH.config(S_IP, S_GW, S_MASK, S_DNS1, S_DNS2))
    LOGE("net", "Ethernet static IP config failed");
  else
    LOGI("net", "Ethernet static IP " + S_IP.toString());
  _ethStart = millis();
  LOGI("net", "bringing up Ethernet (static IP)...");
#else
  _ethStart = millis();
  LOGI("net", "waiting for Ethernet DHCP...");
#endif
}

void NetworkClass::loop() {
  _ethUp = s_ethGotIp;

  // Fallback to WiFi once if Ethernet fails to obtain an IP in time.
  if (!_ethUp && !_wifi && !_fallbackTried &&
      millis() - _ethStart > ETH_LINK_TIMEOUT_MS) {
    _fallbackTried = true;
    LOGW("net", "no Ethernet — falling back to WiFi SSID '" WIFI_FALLBACK_SSID "'");
    WiFi.mode(WIFI_STA);
#if USE_STATIC_IP
    if (!WiFi.config(S_IP, S_GW, S_MASK, S_DNS1, S_DNS2))
      LOGE("net", "WiFi static IP config failed");
#endif
    WiFi.begin(WIFI_FALLBACK_SSID, WIFI_FALLBACK_PASS);
  }

  // If Ethernet recovers while on WiFi, prefer Ethernet again.
  if (_ethUp && _wifi) {
    LOGI("net", "Ethernet restored — disabling WiFi");
    WiFi.disconnect(true);
    _wifi = false;
  }
  if (!_ethUp && _fallbackTried)
    _wifi = (WiFi.status() == WL_CONNECTED);

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
