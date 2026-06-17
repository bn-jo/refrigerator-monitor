/* =====================================================================
 *  Sensors.cpp
 * ===================================================================== */
#include "Sensors.h"
#include "Logger.h"
#include "TimeSync.h"
#include <OneWire.h>
#include <DallasTemperature.h>

SensorsClass Sensors;

static OneWire owFridge(PIN_ONEWIRE_FRIDGE);
static OneWire owOutdoor(PIN_ONEWIRE_OUTDOOR);
static DallasTemperature dsFridge(&owFridge);
static DallasTemperature dsOutdoor(&owOutdoor);

// Discovered ROM addresses, sorted, so fridge[0..4] map stably to physical
// sensors across reboots (DS18B20 enumeration order is address order).
static DeviceAddress fridgeAddr[NUM_FRIDGES];
static DeviceAddress outdoorAddr;
static bool outdoorPresent = false;

static String addrToHex(const DeviceAddress a) {
  char buf[17];
  for (int i = 0; i < 8; i++) sprintf(buf + i * 2, "%02X", a[i]);
  buf[16] = 0;
  return String(buf);
}

// Insertion-sort discovered addresses for a stable index→sensor mapping.
static void sortAddrs(DeviceAddress* arr, int n) {
  for (int i = 1; i < n; i++) {
    DeviceAddress key; memcpy(key, arr[i], 8);
    int j = i - 1;
    while (j >= 0 && memcmp(arr[j], key, 8) > 0) {
      memcpy(arr[j + 1], arr[j], 8); j--;
    }
    memcpy(arr[j + 1], key, 8);
  }
}

void SensorsClass::begin() {
  dsFridge.begin();
  dsOutdoor.begin();
  dsFridge.setResolution(DS18B20_RESOLUTION);
  dsOutdoor.setResolution(DS18B20_RESOLUTION);
  // Non-blocking: we manage the 750 ms conversion delay ourselves.
  dsFridge.setWaitForConversion(false);
  dsOutdoor.setWaitForConversion(false);
  discover();
}

void SensorsClass::discover() {
  fridgeCountDetected = 0;
  owFridge.reset_search();
  DeviceAddress a;
  while (owFridge.search(a) && fridgeCountDetected < NUM_FRIDGES) {
    if (OneWire::crc8(a, 7) == a[7]) memcpy(fridgeAddr[fridgeCountDetected++], a, 8);
  }
  sortAddrs(fridgeAddr, fridgeCountDetected);

  outdoorPresent = false;
  owOutdoor.reset_search();
  if (owOutdoor.search(a) && OneWire::crc8(a, 7) == a[7]) {
    memcpy(outdoorAddr, a, 8); outdoorPresent = true;
  }

  LOGI("sensor", "discovered " + String(fridgeCountDetected) +
       "/5 fridge sensors, outdoor " + (outdoorPresent ? "present" : "MISSING"));

  for (int i = 0; i < NUM_FRIDGES; i++)
    readings[i].romId = (i < fridgeCountDetected) ? addrToHex(fridgeAddr[i]) : "";
  readings[IDX_OUTDOOR].romId = outdoorPresent ? addrToHex(outdoorAddr) : "";
}

void SensorsClass::requestAll() {
  dsFridge.requestTemperatures();
  dsOutdoor.requestTemperatures();
  _convStart = millis();
}

bool SensorsClass::conversionReady() {
  return millis() - _convStart >= 800;  // 12-bit conversion ≈ 750 ms
}

static void evaluate(SensorReading& r, float t) {
  if (t == DS18B20_ERR_VALUE || t == 85.0f) {  // 85 = power-on reset value
    r.state = SENSOR_FAULT;
  } else if (t < TEMP_MIN_VALID || t > TEMP_MAX_VALID) {
    r.state = SENSOR_OUTOFRANGE;
  } else {
    r.tempC = t;
    r.state = SENSOR_OK;
    r.lastValid = TimeSync.now();
  }
}

void SensorsClass::readAll() {
  // Re-discover if the population shrank (hot-unplug) — cheap and robust.
  if (fridgeCountDetected < NUM_FRIDGES) discover();

  for (int i = 0; i < NUM_FRIDGES; i++) {
    if (i >= fridgeCountDetected) { readings[i].state = SENSOR_MISSING; continue; }
    float t = NAN;
    for (int retry = 0; retry < SENSOR_RETRY_MAX; retry++) {
      t = dsFridge.getTempC(fridgeAddr[i]);
      if (t != DS18B20_ERR_VALUE && t != 85.0f) break;
      delay(5);
    }
    SensorState prev = readings[i].state;
    evaluate(readings[i], t);
    if (readings[i].state != SENSOR_OK && prev == SENSOR_OK)
      LOGW("sensor", "fridge " + String(i + 1) + " fault (raw=" + String(t) + ")");
  }

  if (outdoorPresent) {
    float t = dsOutdoor.getTempC(outdoorAddr);
    evaluate(readings[IDX_OUTDOOR], t);
  } else {
    readings[IDX_OUTDOOR].state = SENSOR_MISSING;
  }
}

String SensorsClass::romIdJson() {
  String s = "{";
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (i) s += ",";
    s += "\"" + String(i) + "\":\"" + readings[i].romId + "\"";
  }
  s += "}";
  return s;
}
