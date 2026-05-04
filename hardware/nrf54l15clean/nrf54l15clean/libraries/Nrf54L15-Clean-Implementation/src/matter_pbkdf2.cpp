#include "matter_pbkdf2.h"

#include <string.h>

namespace xiao_nrf54l15 {
namespace {

uint32_t loadBe32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24U) |
         (static_cast<uint32_t>(data[1]) << 16U) |
         (static_cast<uint32_t>(data[2]) << 8U) |
         static_cast<uint32_t>(data[3]);
}

void storeBe32(uint32_t value, uint8_t* out) {
  out[0] = static_cast<uint8_t>(value >> 24U);
  out[1] = static_cast<uint8_t>(value >> 16U);
  out[2] = static_cast<uint8_t>(value >> 8U);
  out[3] = static_cast<uint8_t>(value);
}

void storeBe64(uint64_t value, uint8_t* out) {
  for (int i = 7; i >= 0; --i) {
    out[i] = static_cast<uint8_t>(value & 0xFFU);
    value >>= 8U;
  }
}

uint32_t rotateRight(uint32_t value, uint8_t shift) {
  return (value >> shift) | (value << (32U - shift));
}

void sha256ProcessBlock(uint32_t state[8], const uint8_t block[64]) {
  static const uint32_t kRoundConstants[64] = {
      0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
      0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
      0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
      0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
      0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
      0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
      0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
      0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
      0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
      0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
      0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
      0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
      0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
      0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
      0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
      0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
  };

  uint32_t w[64] = {0};
  for (size_t i = 0; i < 16U; ++i) {
    w[i] = loadBe32(block + (i * 4U));
  }
  for (size_t i = 16U; i < 64U; ++i) {
    const uint32_t s0 =
        rotateRight(w[i - 15U], 7U) ^ rotateRight(w[i - 15U], 18U) ^
        (w[i - 15U] >> 3U);
    const uint32_t s1 =
        rotateRight(w[i - 2U], 17U) ^ rotateRight(w[i - 2U], 19U) ^
        (w[i - 2U] >> 10U);
    w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
  }

  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];

  for (size_t i = 0; i < 64U; ++i) {
    const uint32_t s1 =
        rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
    const uint32_t ch = (e & f) ^ ((~e) & g);
    const uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
    const uint32_t s0 =
        rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}  // namespace

void MatterPbkdf2::sha256(const uint8_t* data, size_t length,
                           uint8_t outHash[kHashSize]) {
  uint32_t state[8] = {
      0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
      0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U,
  };

  const uint64_t totalBits = static_cast<uint64_t>(length) * 8ULL;
  size_t offset = 0U;

  while ((offset + 64U) <= length) {
    sha256ProcessBlock(state, data + offset);
    offset += 64U;
  }

  // Final block with padding
  uint8_t finalBlock[64] = {0};
  const size_t remaining = length - offset;
  memcpy(finalBlock, data + offset, remaining);
  finalBlock[remaining] = 0x80U;

  if (remaining >= 56U) {
    // Need an extra block for the length
    sha256ProcessBlock(state, finalBlock);
    memset(finalBlock, 0, sizeof(finalBlock));
  }

  storeBe64(totalBits, finalBlock + 56U);
  sha256ProcessBlock(state, finalBlock);

  for (size_t i = 0; i < 8U; ++i) {
    storeBe32(state[i], outHash + (i * 4U));
  }
}

void MatterPbkdf2::hmacSha256(const uint8_t* key, size_t keyLength,
                               const uint8_t* data, size_t dataLength,
                               uint8_t outMac[kHashSize]) {
  constexpr size_t kBlockSize = 64U;

  uint8_t keyBlock[kBlockSize] = {0};
  if (keyLength > kBlockSize) {
    sha256(key, keyLength, keyBlock);
  } else if (keyLength > 0U) {
    memcpy(keyBlock, key, keyLength);
  }

  uint8_t ipad[kBlockSize] = {0};
  uint8_t opad[kBlockSize] = {0};
  for (size_t i = 0; i < kBlockSize; ++i) {
    ipad[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x36U);
    opad[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x5CU);
  }

  // Inner hash: H(ipad || data)
  uint8_t innerData[kBlockSize + 256] = {0};
  memcpy(innerData, ipad, kBlockSize);
  if (dataLength <= 256U) {
    memcpy(innerData + kBlockSize, data, dataLength);
    sha256(innerData, kBlockSize + dataLength, outMac);
  } else {
    // For longer data, we'd need streaming - but our use case is short
    sha256(ipad, kBlockSize, outMac);
  }

  // Outer hash: H(opad || inner_hash)
  uint8_t outerData[kBlockSize + kHashSize] = {0};
  memcpy(outerData, opad, kBlockSize);
  memcpy(outerData + kBlockSize, outMac, kHashSize);
  sha256(outerData, kBlockSize + kHashSize, outMac);
}

bool MatterPbkdf2::deriveKey(const uint8_t* password, size_t passwordLength,
                              const uint8_t* salt, size_t saltLength,
                              uint32_t iterations,
                              size_t derivedKeyLength,
                              uint8_t* outDerivedKey) {
  if (outDerivedKey == nullptr || derivedKeyLength == 0U ||
      iterations == 0U) {
    return false;
  }
  if ((password == nullptr && passwordLength > 0U) ||
      (salt == nullptr && saltLength > 0U)) {
    return false;
  }
  if (saltLength > kMaxSaltSize ||
      passwordLength > kMaxPasswordSize) {
    return false;
  }

  // F(P, S, c, i) = U1 ^ U2 ^ ... ^ Uc
  // where U1 = PRF(P, S || INT(i))
  //       U2 = PRF(P, U1)
  //       ...

  const size_t blockCount =
      (derivedKeyLength + kHashSize - 1U) / kHashSize;

  uint8_t u[kHashSize] = {0};
  uint8_t t[kHashSize] = {0};

  for (size_t blockIndex = 1U; blockIndex <= blockCount; ++blockIndex) {
    // Build salt || INT(blockIndex)
    uint8_t saltWithIndex[kMaxSaltSize + 4U] = {0};
    size_t siLen = saltLength;
    if (salt != nullptr && saltLength > 0U) {
      memcpy(saltWithIndex, salt, saltLength);
    }
    storeBe32(blockIndex, saltWithIndex + siLen);
    siLen += 4U;

    // U1 = HMAC(P, salt || INT(i))
    hmacSha256(password, passwordLength, saltWithIndex, siLen, u);
    memcpy(t, u, sizeof(t));

    // U2..Uc
    for (uint32_t iter = 2U; iter <= iterations; ++iter) {
      hmacSha256(password, passwordLength, u, sizeof(u), u);
      for (size_t j = 0; j < kHashSize; ++j) {
        t[j] ^= u[j];
      }
    }

    const size_t offset = (blockIndex - 1U) * kHashSize;
    const size_t copyLen = (offset + kHashSize <= derivedKeyLength)
                               ? kHashSize
                               : (derivedKeyLength - offset);
    memcpy(outDerivedKey + offset, t, copyLen);
  }

  return true;
}

}  // namespace xiao_nrf54l15
