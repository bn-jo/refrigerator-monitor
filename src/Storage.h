/* =====================================================================
 *  Storage.h — fixed-size circular history database on LittleFS
 *
 *  Design rationale (see docs/ARCHITECTURE.md "Data Storage"):
 *    - A single pre-allocated, fixed-size binary file is used as a ring
 *      buffer of HISTORY_CAPACITY records (30 days @ 1/min = 43200).
 *    - Pre-allocation means flash never grows/fragments at runtime, so
 *      there is no "disk full" failure mode and wear is evenly spread.
 *    - Each record carries a CRC8 so corruption (e.g. power loss during a
 *      write) is detected and the record is skipped on read.
 *    - A small header holds head index + count and a CRC; it is rewritten
 *      after each append. If the header is corrupt it is rebuilt by scan.
 * ===================================================================== */
#pragma once
#include <Arduino.h>
#include <FS.h>
#include "config.h"

#pragma pack(push, 1)
struct HistoryRecord {
  uint32_t ts;                 // unix time (local-epoch seconds)
  int16_t  temp[NUM_FRIDGES];  // °C × 100 (centi-degrees); INT16_MIN = invalid
  uint8_t  crc;                // CRC8 over the preceding bytes
};
#pragma pack(pop)

struct AggPoint { uint32_t ts; float temp[NUM_FRIDGES]; };

class StorageClass {
public:
  bool   begin();
  bool   append(time_t ts, const float temps[NUM_FRIDGES]);
  size_t recordCount() const { return _count; }
  bool   healthy() const { return _ok; }
  uint32_t writeFailures() const { return _writeFail; }

  // Stream a downsampled JSON series for [now-rangeSeconds, now] into `out`.
  // maxPoints caps the number of buckets returned for smooth rendering.
  String queryJson(uint32_t rangeSeconds, uint16_t maxPoints);

private:
  bool   readHeader();
  bool   writeHeader();
  bool   rebuildFromScan();
  bool   readRecord(size_t slot, HistoryRecord& r);

  File   _f;
  bool   _ok = false;
  size_t _head = 0;            // next slot to write
  size_t _count = 0;           // valid records (<= HISTORY_CAPACITY)
  uint32_t _writeFail = 0;
  SemaphoreHandle_t _mtx = nullptr;
};

extern StorageClass Storage;
