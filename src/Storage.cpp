/* =====================================================================
 *  Storage.cpp — circular history database implementation
 * ===================================================================== */
#include "Storage.h"
#include "Logger.h"
#include <LittleFS.h>

StorageClass Storage;

static const uint32_t HDR_MAGIC = 0x46524947;  // "FRIG"
#pragma pack(push, 1)
struct DbHeader {
  uint32_t magic;
  uint32_t head;
  uint32_t count;
  uint32_t crc;     // crc32 of the preceding fields
};
#pragma pack(pop)

static const size_t HDR_SIZE = sizeof(DbHeader);
static const size_t REC_SIZE = sizeof(HistoryRecord);
static const int16_t TEMP_INVALID = INT16_MIN;

// ---- CRC helpers ----------------------------------------------------
static uint8_t crc8(const uint8_t* p, size_t n) {
  uint8_t crc = 0;
  while (n--) {
    crc ^= *p++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : crc << 1;
  }
  return crc;
}
static uint32_t crc32(const uint8_t* p, size_t n) {
  uint32_t crc = 0xFFFFFFFF;
  while (n--) {
    crc ^= *p++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
  }
  return ~crc;
}

static int16_t toCenti(float c) {
  if (isnan(c) || c < TEMP_MIN_VALID || c > TEMP_MAX_VALID) return TEMP_INVALID;
  return (int16_t)lroundf(c * 100.0f);
}

// ---------------------------------------------------------------------
bool StorageClass::begin() {
  _mtx = xSemaphoreCreateMutex();

  bool exists = LittleFS.exists(HISTORY_FILE);
  _f = LittleFS.open(HISTORY_FILE, exists ? "r+" : "w+");
  if (!_f) { LOGE("storage", "cannot open history file"); _ok = false; return false; }

  // The ring file is NOT bulk pre-allocated (that was slow on first boot and
  // a brownout risk). Instead it grows lazily: appends are always sequential
  // (head increments by one) until the 30-day wrap, so each write lands
  // exactly at end-of-file. After the first full cycle the file is at its
  // final size (~648 KB) and slots are simply overwritten in place.
  if (!exists) {
    _head = 0; _count = 0;
    if (!writeHeader()) { LOGE("storage", "init header write failed"); _ok = false; return false; }
    LOGI("storage", "history database created (grows lazily to ~" +
                    String((HDR_SIZE + (size_t)HISTORY_CAPACITY * REC_SIZE) / 1024) +
                    " KB over 30 days)");
  } else if (!readHeader()) {
    LOGW("storage", "header invalid — rebuilding by scan");
    rebuildFromScan();
  }
  _ok = true;
  LOGI("storage", "history ready: " + String(_count) + " records");
  return true;
}

bool StorageClass::readHeader() {
  _f.seek(0);
  DbHeader h{};
  if (_f.read((uint8_t*)&h, HDR_SIZE) != (int)HDR_SIZE) return false;
  if (h.magic != HDR_MAGIC) return false;
  if (h.crc != crc32((uint8_t*)&h, HDR_SIZE - 4)) return false;
  if (h.head >= HISTORY_CAPACITY || h.count > HISTORY_CAPACITY) return false;
  _head = h.head; _count = h.count;
  return true;
}

bool StorageClass::writeHeader() {
  _f.seek(0);
  DbHeader h{ HDR_MAGIC, (uint32_t)_head, (uint32_t)_count, 0 };
  h.crc = crc32((uint8_t*)&h, HDR_SIZE - 4);
  if (_f.write((uint8_t*)&h, HDR_SIZE) != (int)HDR_SIZE) return false;
  _f.flush();
  return true;
}

// Recover head/count by scanning valid records and their timestamps.
bool StorageClass::rebuildFromScan() {
  size_t valid = 0, newest = 0; uint32_t newestTs = 0;
  for (size_t i = 0; i < HISTORY_CAPACITY; i++) {
    HistoryRecord r;
    if (!readRecord(i, r) || r.ts == 0) continue;
    valid++;
    if (r.ts >= newestTs) { newestTs = r.ts; newest = i; }
  }
  _count = valid;
  _head  = (newest + 1) % HISTORY_CAPACITY;
  writeHeader();
  return true;
}

bool StorageClass::readRecord(size_t slot, HistoryRecord& r) {
  _f.seek(HDR_SIZE + slot * REC_SIZE);
  if (_f.read((uint8_t*)&r, REC_SIZE) != (int)REC_SIZE) return false;
  return r.crc == crc8((uint8_t*)&r, REC_SIZE - 1);
}

bool StorageClass::append(time_t ts, const float temps[NUM_FRIDGES]) {
  if (!_ok) return false;
  HistoryRecord r{};
  r.ts = (uint32_t)ts;
  for (int i = 0; i < NUM_FRIDGES; i++) r.temp[i] = toCenti(temps[i]);
  r.crc = crc8((uint8_t*)&r, REC_SIZE - 1);

  xSemaphoreTake(_mtx, portMAX_DELAY);
  _f.seek(HDR_SIZE + _head * REC_SIZE);
  bool ok = _f.write((uint8_t*)&r, REC_SIZE) == (int)REC_SIZE;
  if (ok) {
    _f.flush();
    _head = (_head + 1) % HISTORY_CAPACITY;
    if (_count < HISTORY_CAPACITY) _count++;
    ok = writeHeader();     // commit head/count after the data is durable
  }
  if (!ok) { _writeFail++; LOGE("storage", "history write failed"); }
  xSemaphoreGive(_mtx);
  return ok;
}

// Downsample to <= maxPoints buckets by time-averaging each bucket.
String StorageClass::queryJson(uint32_t rangeSeconds, uint16_t maxPoints) {
  uint32_t now = (uint32_t)time(nullptr);
  uint32_t from = (rangeSeconds >= now) ? 0 : now - rangeSeconds;
  if (maxPoints == 0) maxPoints = 1;
  uint32_t bucketSec = rangeSeconds / maxPoints;
  if (bucketSec < 1) bucketSec = 1;

  // Accumulators per bucket.
  uint16_t nB = maxPoints + 1;
  // Allocate on heap to avoid large stack frames.
  double* sum = (double*)calloc((size_t)nB * NUM_FRIDGES, sizeof(double));
  uint16_t* cnt = (uint16_t*)calloc((size_t)nB * NUM_FRIDGES, sizeof(uint16_t));
  if (!sum || !cnt) { free(sum); free(cnt); return "{\"error\":\"oom\"}"; }

  xSemaphoreTake(_mtx, portMAX_DELAY);
  for (size_t k = 0; k < _count; k++) {
    size_t slot = (_head + HISTORY_CAPACITY - _count + k) % HISTORY_CAPACITY;
    HistoryRecord r;
    if (!readRecord(slot, r) || r.ts < from || r.ts > now) continue;
    uint16_t b = (uint16_t)((r.ts - from) / bucketSec);
    if (b >= nB) b = nB - 1;
    for (int i = 0; i < NUM_FRIDGES; i++) {
      if (r.temp[i] == TEMP_INVALID) continue;
      sum[b * NUM_FRIDGES + i] += r.temp[i] / 100.0;
      cnt[b * NUM_FRIDGES + i]++;
    }
  }
  xSemaphoreGive(_mtx);

  String out = "{\"from\":" + String(from) + ",\"to\":" + String(now) +
               ",\"bucket\":" + String(bucketSec) + ",\"points\":[";
  bool first = true;
  for (uint16_t b = 0; b < nB; b++) {
    // Skip empty buckets entirely (keeps payload small).
    bool any = false;
    for (int i = 0; i < NUM_FRIDGES; i++) if (cnt[b * NUM_FRIDGES + i]) any = true;
    if (!any) continue;
    if (!first) out += ",";
    first = false;
    out += "{\"t\":" + String(from + (uint32_t)b * bucketSec) + ",\"v\":[";
    for (int i = 0; i < NUM_FRIDGES; i++) {
      if (i) out += ",";
      uint16_t c = cnt[b * NUM_FRIDGES + i];
      if (c) out += String(sum[b * NUM_FRIDGES + i] / c, 2);
      else   out += "null";
    }
    out += "]}";
  }
  out += "]}";
  free(sum); free(cnt);
  return out;
}
