#pragma once

// Minimal Matter core-stage shim for staged upstream support units that only
// need fixed-width big-endian helpers, not the full nlbyteorder/nlio stack.

#include <stdint.h>

namespace chip {
namespace Encoding {
namespace BigEndian {

inline void Put16(uint8_t* p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value >> 8);
  p[1] = static_cast<uint8_t>(value);
}

inline void Put32(uint8_t* p, uint32_t value) {
  p[0] = static_cast<uint8_t>(value >> 24);
  p[1] = static_cast<uint8_t>(value >> 16);
  p[2] = static_cast<uint8_t>(value >> 8);
  p[3] = static_cast<uint8_t>(value);
}

inline void Put64(uint8_t* p, uint64_t value) {
  p[0] = static_cast<uint8_t>(value >> 56);
  p[1] = static_cast<uint8_t>(value >> 48);
  p[2] = static_cast<uint8_t>(value >> 40);
  p[3] = static_cast<uint8_t>(value >> 32);
  p[4] = static_cast<uint8_t>(value >> 24);
  p[5] = static_cast<uint8_t>(value >> 16);
  p[6] = static_cast<uint8_t>(value >> 8);
  p[7] = static_cast<uint8_t>(value);
}

inline uint16_t Get16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                               static_cast<uint16_t>(p[1]));
}

inline uint32_t Get32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

inline uint64_t Get64(const uint8_t* p) {
  return (static_cast<uint64_t>(p[0]) << 56) |
         (static_cast<uint64_t>(p[1]) << 48) |
         (static_cast<uint64_t>(p[2]) << 40) |
         (static_cast<uint64_t>(p[3]) << 32) |
         (static_cast<uint64_t>(p[4]) << 24) |
         (static_cast<uint64_t>(p[5]) << 16) |
         (static_cast<uint64_t>(p[6]) << 8) |
         static_cast<uint64_t>(p[7]);
}

}  // namespace BigEndian
}  // namespace Encoding
}  // namespace chip
