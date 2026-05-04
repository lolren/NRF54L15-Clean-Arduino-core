#pragma once

#include <stddef.h>
#include <stdint.h>

namespace xiao_nrf54l15 {

// PBKDF2-HMAC-SHA256 implementation for Matter passcode derivation.
// Used to derive the SPAKE2+ w0 and w1 values from the setup PIN code.

class MatterPbkdf2 {
 public:
  static constexpr size_t kHashSize = 32U;
  static constexpr size_t kMaxSaltSize = 64U;
  static constexpr size_t kMaxPasswordSize = 128U;

  // Derive key using PBKDF2-HMAC-SHA256
  // password, passwordLength: the passcode (PIN) as bytes
  // salt, saltLength: the salt (typically the SPAKE2+ salt)
  // iterations: iteration count (Matter uses 2000 for PASE)
  // derivedKeyLength: desired output length
  // outDerivedKey: output buffer
  static bool deriveKey(const uint8_t* password, size_t passwordLength,
                        const uint8_t* salt, size_t saltLength,
                        uint32_t iterations,
                        size_t derivedKeyLength,
                        uint8_t* outDerivedKey);

  // SHA-256 hash function
  static void sha256(const uint8_t* data, size_t length,
                     uint8_t outHash[kHashSize]);

  // HMAC-SHA256
  static void hmacSha256(const uint8_t* key, size_t keyLength,
                         const uint8_t* data, size_t dataLength,
                         uint8_t outMac[kHashSize]);

 private:
  static void sha256Transform(uint32_t state[8], const uint8_t block[64]);
  static void sha256Pad(uint64_t totalBits, uint8_t* block, size_t* outLength);
};

}  // namespace xiao_nrf54l15
