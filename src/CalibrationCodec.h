#pragma once

#include <stdint.h>

// Versioned, checksummed representation of the touch calibration file. This
// header intentionally has no Arduino/FS dependency so the format can be
// regression-tested on the native PlatformIO target.
namespace CalibrationCodec {

static constexpr uint32_t kMagic = 0x314C4143UL; // "CAL1"
static constexpr uint16_t kVersion = 1;

struct Record {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  int32_t offset;
  int32_t range;
  uint32_t checksum;
};

static_assert(sizeof(Record) == 20, "Calibration record layout changed");

inline uint32_t Checksum(int32_t offset, int32_t range) {
  // FNV-1a over the two values, independent of struct padding/endianness.
  uint32_t hash = 2166136261UL;
  const uint32_t values[] = {static_cast<uint32_t>(offset),
                             static_cast<uint32_t>(range)};
  for (uint32_t value : values) {
    for (uint8_t byte = 0; byte < 4; ++byte) {
      hash ^= (value >> (byte * 8)) & 0xFFU;
      hash *= 16777619UL;
    }
  }
  return hash;
}

inline Record Make(int32_t offset, int32_t range) {
  return {kMagic, kVersion, 0, offset, range, Checksum(offset, range)};
}

inline bool IsValid(const Record& record) {
  return record.magic == kMagic && record.version == kVersion &&
         record.range > 0 &&
         record.checksum == Checksum(record.offset, record.range);
}

} // namespace CalibrationCodec
