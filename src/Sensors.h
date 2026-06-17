/* =====================================================================
 *  Sensors.h — DS18B20 acquisition (5 fridges on one bus + 1 outdoor)
 * ===================================================================== */
#pragma once
#include <Arduino.h>
#include "config.h"

enum SensorState { SENSOR_OK, SENSOR_FAULT, SENSOR_MISSING, SENSOR_OUTOFRANGE };

struct SensorReading {
  float        tempC = NAN;
  SensorState  state = SENSOR_MISSING;
  time_t       lastValid = 0;
  String       romId;          // unique 64-bit ROM address (hex)
};

class SensorsClass {
public:
  void begin();
  void requestAll();           // start async conversion on both buses
  bool conversionReady();      // true once the 750 ms conversion elapsed
  void readAll();              // populate readings[] from the buses

  SensorReading readings[NUM_SENSORS];   // [0..4]=fridges, [5]=outdoor
  uint8_t fridgeCountDetected = 0;       // sensors enumerated on fridge bus

  String romIdJson();          // JSON map of discovered ROM IDs

private:
  uint32_t _convStart = 0;
  void discover();
};

extern SensorsClass Sensors;
