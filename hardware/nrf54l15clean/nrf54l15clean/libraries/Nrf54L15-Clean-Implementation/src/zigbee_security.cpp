#include "zigbee_security.h"

#include <string.h>

namespace xiao_nrf54l15 {

namespace {

constexpr uint8_t kZigbeeNwkProtocolVersion = 2U;
constexpr uint8_t kCcmMicLength = 4U;
constexpr uint8_t kCcmNonceLength = 13U;
constexpr uint8_t kCcmLengthFieldSize = 2U;
constexpr uint8_t kInstallCodeMaxLength = 18U;

using AesState = uint8_t[4][4];

constexpr uint8_t kAesSbox[256] = {
    0x63U, 0x7CU, 0x77U, 0x7BU, 0xF2U, 0x6BU, 0x6FU, 0xC5U, 0x30U, 0x01U,
    0x67U, 0x2BU, 0xFEU, 0xD7U, 0xABU, 0x76U, 0xCAU, 0x82U, 0xC9U, 0x7DU,
    0xFAU, 0x59U, 0x47U, 0xF0U, 0xADU, 0xD4U, 0xA2U, 0xAFU, 0x9CU, 0xA4U,
    0x72U, 0xC0U, 0xB7U, 0xFDU, 0x93U, 0x26U, 0x36U, 0x3FU, 0xF7U, 0xCCU,
    0x34U, 0xA5U, 0xE5U, 0xF1U, 0x71U, 0xD8U, 0x31U, 0x15U, 0x04U, 0xC7U,
    0x23U, 0xC3U, 0x18U, 0x96U, 0x05U, 0x9AU, 0x07U, 0x12U, 0x80U, 0xE2U,
    0xEBU, 0x27U, 0xB2U, 0x75U, 0x09U, 0x83U, 0x2CU, 0x1AU, 0x1BU, 0x6EU,
    0x5AU, 0xA0U, 0x52U, 0x3BU, 0xD6U, 0xB3U, 0x29U, 0xE3U, 0x2FU, 0x84U,
    0x53U, 0xD1U, 0x00U, 0xEDU, 0x20U, 0xFCU, 0xB1U, 0x5BU, 0x6AU, 0xCBU,
    0xBEU, 0x39U, 0x4AU, 0x4CU, 0x58U, 0xCFU, 0xD0U, 0xEFU, 0xAAU, 0xFBU,
    0x43U, 0x4DU, 0x33U, 0x85U, 0x45U, 0xF9U, 0x02U, 0x7FU, 0x50U, 0x3CU,
    0x9FU, 0xA8U, 0x51U, 0xA3U, 0x40U, 0x8FU, 0x92U, 0x9DU, 0x38U, 0xF5U,
    0xBCU, 0xB6U, 0xDAU, 0x21U, 0x10U, 0xFFU, 0xF3U, 0xD2U, 0xCDU, 0x0CU,
    0x13U, 0xECU, 0x5FU, 0x97U, 0x44U, 0x17U, 0xC4U, 0xA7U, 0x7EU, 0x3DU,
    0x64U, 0x5DU, 0x19U, 0x73U, 0x60U, 0x81U, 0x4FU, 0xDCU, 0x22U, 0x2AU,
    0x90U, 0x88U, 0x46U, 0xEEU, 0xB8U, 0x14U, 0xDEU, 0x5EU, 0x0BU, 0xDBU,
    0xE0U, 0x32U, 0x3AU, 0x0AU, 0x49U, 0x06U, 0x24U, 0x5CU, 0xC2U, 0xD3U,
    0xACU, 0x62U, 0x91U, 0x95U, 0xE4U, 0x79U, 0xE7U, 0xC8U, 0x37U, 0x6DU,
    0x8DU, 0xD5U, 0x4EU, 0xA9U, 0x6CU, 0x56U, 0xF4U, 0xEAU, 0x65U, 0x7AU,
    0xAEU, 0x08U, 0xBAU, 0x78U, 0x25U, 0x2EU, 0x1CU, 0xA6U, 0xB4U, 0xC6U,
    0xE8U, 0xDDU, 0x74U, 0x1FU, 0x4BU, 0xBDU, 0x8BU, 0x8AU, 0x70U, 0x3EU,
    0xB5U, 0x66U, 0x48U, 0x03U, 0xF6U, 0x0EU, 0x61U, 0x35U, 0x57U, 0xB9U,
    0x86U, 0xC1U, 0x1DU, 0x9EU, 0xE1U, 0xF8U, 0x98U, 0x11U, 0x69U, 0xD9U,
    0x8EU, 0x94U, 0x9BU, 0x1EU, 0x87U, 0xE9U, 0xCEU, 0x55U, 0x28U, 0xDFU,
    0x8CU, 0xA1U, 0x89U, 0x0DU, 0xBFU, 0xE6U, 0x42U, 0x68U, 0x41U, 0x99U,
    0x2DU, 0x0FU, 0xB0U, 0x54U, 0xBBU, 0x16U};

constexpr uint8_t kAesRcon[11] = {0x00U, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U,
                                  0x20U, 0x40U, 0x80U, 0x1BU, 0x36U};

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8U);
}

uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8U) |
         (static_cast<uint32_t>(data[2]) << 16U) |
         (static_cast<uint32_t>(data[3]) << 24U);
}

uint64_t readLe64(const uint8_t* data) {
  uint64_t value = 0U;
  for (uint8_t i = 0U; i < 8U; ++i) {
    value |= (static_cast<uint64_t>(data[i]) << (8U * i));
  }
  return value;
}

void writeLe16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value & 0xFFU);
  data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeLe32(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value & 0xFFU);
  data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  data[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
  data[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

void writeLe64(uint8_t* data, uint64_t value) {
  for (uint8_t i = 0U; i < 8U; ++i) {
    data[i] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
  }
}

bool appendBytes(uint8_t* out, uint8_t capacity, uint8_t* offset,
                 const uint8_t* data, uint8_t length) {
  if (out == nullptr || offset == nullptr) {
    return false;
  }
  if (length > 0U && data == nullptr) {
    return false;
  }
  if (capacity < static_cast<uint8_t>(*offset + length)) {
    return false;
  }
  if (length > 0U) {
    memcpy(&out[*offset], data, length);
  }
  *offset = static_cast<uint8_t>(*offset + length);
  return true;
}

bool appendLe16(uint8_t* out, uint8_t capacity, uint8_t* offset,
                uint16_t value) {
  uint8_t temp[2] = {0U};
  writeLe16(temp, value);
  return appendBytes(out, capacity, offset, temp, sizeof(temp));
}

bool appendLe32(uint8_t* out, uint8_t capacity, uint8_t* offset,
                uint32_t value) {
  uint8_t temp[4] = {0U};
  writeLe32(temp, value);
  return appendBytes(out, capacity, offset, temp, sizeof(temp));
}

bool appendLe64(uint8_t* out, uint8_t capacity, uint8_t* offset,
                uint64_t value) {
  uint8_t temp[8] = {0U};
  writeLe64(temp, value);
  return appendBytes(out, capacity, offset, temp, sizeof(temp));
}

uint8_t aesXtime(uint8_t x) {
  return static_cast<uint8_t>((x << 1U) ^ (((x >> 7U) & 1U) * 0x1BU));
}

void aesAddRoundKey(uint8_t round, AesState* state,
                    const uint8_t* roundKey) {
  for (uint8_t col = 0U; col < 4U; ++col) {
    for (uint8_t row = 0U; row < 4U; ++row) {
      (*state)[row][col] ^= roundKey[(round * 16U) + (col * 4U) + row];
    }
  }
}

void aesSubBytes(AesState* state) {
  for (uint8_t row = 0U; row < 4U; ++row) {
    for (uint8_t col = 0U; col < 4U; ++col) {
      (*state)[row][col] = kAesSbox[(*state)[row][col]];
    }
  }
}

void aesShiftRows(AesState* state) {
  uint8_t temp = (*state)[1][0];
  (*state)[1][0] = (*state)[1][1];
  (*state)[1][1] = (*state)[1][2];
  (*state)[1][2] = (*state)[1][3];
  (*state)[1][3] = temp;

  temp = (*state)[2][0];
  (*state)[2][0] = (*state)[2][2];
  (*state)[2][2] = temp;
  temp = (*state)[2][1];
  (*state)[2][1] = (*state)[2][3];
  (*state)[2][3] = temp;

  temp = (*state)[3][3];
  (*state)[3][3] = (*state)[3][2];
  (*state)[3][2] = (*state)[3][1];
  (*state)[3][1] = (*state)[3][0];
  (*state)[3][0] = temp;
}

void aesMixColumns(AesState* state) {
  for (uint8_t col = 0U; col < 4U; ++col) {
    const uint8_t s0 = (*state)[0][col];
    const uint8_t s1 = (*state)[1][col];
    const uint8_t s2 = (*state)[2][col];
    const uint8_t s3 = (*state)[3][col];
    const uint8_t t = static_cast<uint8_t>(s0 ^ s1 ^ s2 ^ s3);

    uint8_t tm = static_cast<uint8_t>(s0 ^ s1);
    tm = aesXtime(tm);
    (*state)[0][col] ^= static_cast<uint8_t>(tm ^ t);

    tm = static_cast<uint8_t>(s1 ^ s2);
    tm = aesXtime(tm);
    (*state)[1][col] ^= static_cast<uint8_t>(tm ^ t);

    tm = static_cast<uint8_t>(s2 ^ s3);
    tm = aesXtime(tm);
    (*state)[2][col] ^= static_cast<uint8_t>(tm ^ t);

    tm = static_cast<uint8_t>(s3 ^ s0);
    tm = aesXtime(tm);
    (*state)[3][col] ^= static_cast<uint8_t>(tm ^ t);
  }
}

void aesKeyExpansion128(const uint8_t key[16], uint8_t roundKey[176]) {
  memcpy(roundKey, key, 16U);
  uint8_t bytesGenerated = 16U;
  uint8_t rconIteration = 1U;
  uint8_t temp[4] = {0U};

  while (bytesGenerated < 176U) {
    for (uint8_t i = 0U; i < 4U; ++i) {
      temp[i] = roundKey[bytesGenerated - 4U + i];
    }

    if ((bytesGenerated % 16U) == 0U) {
      const uint8_t temp0 = temp[0];
      temp[0] = temp[1];
      temp[1] = temp[2];
      temp[2] = temp[3];
      temp[3] = temp0;

      temp[0] = kAesSbox[temp[0]];
      temp[1] = kAesSbox[temp[1]];
      temp[2] = kAesSbox[temp[2]];
      temp[3] = kAesSbox[temp[3]];
      temp[0] ^= kAesRcon[rconIteration++];
    }

    for (uint8_t i = 0U; i < 4U; ++i) {
      roundKey[bytesGenerated] =
          static_cast<uint8_t>(roundKey[bytesGenerated - 16U] ^ temp[i]);
      ++bytesGenerated;
    }
  }
}

bool aesEncryptBlock(const uint8_t key[16], const uint8_t plaintext[16],
                     uint8_t out[16]) {
  if (key == nullptr || plaintext == nullptr || out == nullptr) {
    return false;
  }

  uint8_t roundKey[176] = {0U};
  uint8_t stateBuffer[16] = {0U};
  aesKeyExpansion128(key, roundKey);
  memcpy(stateBuffer, plaintext, sizeof(stateBuffer));

  auto* state = reinterpret_cast<AesState*>(&stateBuffer[0]);
  aesAddRoundKey(0U, state, roundKey);
  for (uint8_t round = 1U; round < 10U; ++round) {
    aesSubBytes(state);
    aesShiftRows(state);
    aesMixColumns(state);
    aesAddRoundKey(round, state, roundKey);
  }
  aesSubBytes(state);
  aesShiftRows(state);
  aesAddRoundKey(10U, state, roundKey);
  memcpy(out, stateBuffer, sizeof(stateBuffer));
  return true;
}

void xor16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
  for (uint8_t i = 0U; i < 16U; ++i) {
    out[i] = static_cast<uint8_t>(a[i] ^ b[i]);
  }
}

void buildCtrBlock(const uint8_t nonce[kCcmNonceLength], uint16_t counter,
                   uint8_t out[16]) {
  memset(out, 0, 16U);
  out[0] = 0x01U;
  memcpy(&out[1], nonce, kCcmNonceLength);
  out[14] = static_cast<uint8_t>((counter >> 8U) & 0xFFU);
  out[15] = static_cast<uint8_t>(counter & 0xFFU);
}

bool computeCcmMic(const uint8_t key[16], const uint8_t nonce[13],
                   const uint8_t* aad, uint8_t aadLength,
                   const uint8_t* plaintext, uint8_t plaintextLength,
                   uint8_t outMic[kCcmMicLength]) {
  if (key == nullptr || nonce == nullptr || outMic == nullptr) {
    return false;
  }
  if (aadLength > 0U && aad == nullptr) {
    return false;
  }
  if (plaintextLength > 0U && plaintext == nullptr) {
    return false;
  }

  uint8_t x[16] = {0U};
  uint8_t block[16] = {0U};
  uint8_t temp[16] = {0U};

  block[0] = static_cast<uint8_t>(((aadLength > 0U) ? 0x40U : 0x00U) | 0x08U |
                                  (kCcmLengthFieldSize - 1U));
  memcpy(&block[1], nonce, kCcmNonceLength);
  block[14] = static_cast<uint8_t>((plaintextLength >> 8U) & 0xFFU);
  block[15] = plaintextLength;
  if (!aesEncryptBlock(key, block, x)) {
    return false;
  }

  if (aadLength > 0U) {
    memset(block, 0, sizeof(block));
    block[0] = 0x00U;
    block[1] = aadLength;
    const uint8_t firstChunk = (aadLength < 14U) ? aadLength : 14U;
    memcpy(&block[2], aad, firstChunk);
    xor16(x, block, temp);
    if (!aesEncryptBlock(key, temp, x)) {
      return false;
    }

    uint8_t offset = firstChunk;
    while (offset < aadLength) {
      memset(block, 0, sizeof(block));
      const uint8_t remaining = static_cast<uint8_t>(aadLength - offset);
      const uint8_t chunk = (remaining < 16U) ? remaining : 16U;
      memcpy(block, &aad[offset], chunk);
      xor16(x, block, temp);
      if (!aesEncryptBlock(key, temp, x)) {
        return false;
      }
      offset = static_cast<uint8_t>(offset + chunk);
    }
  }

  uint8_t offset = 0U;
  while (offset < plaintextLength) {
    memset(block, 0, sizeof(block));
    const uint8_t remaining = static_cast<uint8_t>(plaintextLength - offset);
    const uint8_t chunk = (remaining < 16U) ? remaining : 16U;
    memcpy(block, &plaintext[offset], chunk);
    xor16(x, block, temp);
    if (!aesEncryptBlock(key, temp, x)) {
      return false;
    }
    offset = static_cast<uint8_t>(offset + chunk);
  }

  uint8_t s0[16] = {0U};
  buildCtrBlock(nonce, 0U, block);
  if (!aesEncryptBlock(key, block, s0)) {
    return false;
  }
  for (uint8_t i = 0U; i < kCcmMicLength; ++i) {
    outMic[i] = static_cast<uint8_t>(x[i] ^ s0[i]);
  }
  return true;
}

uint16_t calculateInstallCodeCrcX25(const uint8_t* installCode,
                                    uint8_t lengthWithoutCrc) {
  if (installCode == nullptr) {
    return 0U;
  }

  uint16_t crc = 0xFFFFU;
  for (uint8_t i = 0U; i < lengthWithoutCrc; ++i) {
    crc ^= installCode[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      if ((crc & 0x0001U) != 0U) {
        crc = static_cast<uint16_t>((crc >> 1U) ^ 0x8408U);
      } else {
        crc = static_cast<uint16_t>(crc >> 1U);
      }
    }
  }
  return static_cast<uint16_t>(crc ^ 0xFFFFU);
}

bool isSupportedInstallCodeLength(uint8_t length) {
  return length == 8U || length == 10U || length == 14U || length == 18U;
}

bool mmoHashInstallCode(const uint8_t* installCode, uint8_t length,
                        uint8_t outKey[16]) {
  if (installCode == nullptr || outKey == nullptr ||
      !isSupportedInstallCodeLength(length) || length > kInstallCodeMaxLength) {
    return false;
  }

  uint8_t padded[32] = {0U};
  uint8_t offset = 0U;
  memcpy(padded, installCode, length);
  offset = length;
  padded[offset++] = 0x80U;
  while ((offset % 16U) != 14U) {
    padded[offset++] = 0U;
  }
  const uint16_t bitLength = static_cast<uint16_t>(length * 8U);
  padded[offset++] = static_cast<uint8_t>((bitLength >> 8U) & 0xFFU);
  padded[offset++] = static_cast<uint8_t>(bitLength & 0xFFU);

  uint8_t hash[16] = {0U};
  uint8_t encrypted[16] = {0U};
  for (uint8_t blockOffset = 0U; blockOffset < offset; blockOffset += 16U) {
    if (!aesEncryptBlock(hash, &padded[blockOffset], encrypted)) {
      return false;
    }
    for (uint8_t i = 0U; i < 16U; ++i) {
      hash[i] = static_cast<uint8_t>(encrypted[i] ^ padded[blockOffset + i]);
    }
  }
  memcpy(outKey, hash, 16U);
  return true;
}

}  // namespace

bool ZigbeeSecurity::loadZigbeeAlliance09LinkKey(uint8_t outKey[16]) {
  if (outKey == nullptr) {
    return false;
  }
  static const uint8_t kAlliance09Key[16] = {'Z', 'i', 'g', 'B', 'e', 'e',
                                             'A', 'l', 'l', 'i', 'a', 'n',
                                             'c', 'e', '0', '9'};
  memcpy(outKey, kAlliance09Key, sizeof(kAlliance09Key));
  return true;
}

uint16_t ZigbeeSecurity::calculateInstallCodeCrc(const uint8_t* installCode,
                                                 uint8_t lengthWithoutCrc) {
  return calculateInstallCodeCrcX25(installCode, lengthWithoutCrc);
}

bool ZigbeeSecurity::validateInstallCode(const uint8_t* installCode,
                                         uint8_t length) {
  if (installCode == nullptr || !isSupportedInstallCodeLength(length)) {
    return false;
  }
  const uint8_t payloadLength = static_cast<uint8_t>(length - 2U);
  const uint16_t expectedCrc =
      calculateInstallCodeCrcX25(installCode, payloadLength);
  return readLe16(&installCode[payloadLength]) == expectedCrc;
}

bool ZigbeeSecurity::deriveInstallCodeLinkKey(const uint8_t* installCode,
                                              uint8_t length,
                                              uint8_t outKey[16]) {
  if (!validateInstallCode(installCode, length)) {
    return false;
  }
  return mmoHashInstallCode(installCode, length, outKey);
}

bool ZigbeeSecurity::buildNwkNonce(uint64_t sourceIeee, uint32_t frameCounter,
                                   uint8_t securityControl,
                                   uint8_t outNonce[13]) {
  if (outNonce == nullptr) {
    return false;
  }
  writeLe64(&outNonce[0], sourceIeee);
  writeLe32(&outNonce[8], frameCounter);
  outNonce[12] = securityControl;
  return true;
}

bool ZigbeeSecurity::buildNwkSecurityHeader(
    const ZigbeeNwkSecurityHeader& header, uint8_t* outHeader,
    uint8_t* outHeaderLength) {
  if (outHeader == nullptr || outHeaderLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outHeader, 32U, &offset, &header.securityControl, 1U) ||
      !appendLe32(outHeader, 32U, &offset, header.frameCounter)) {
    return false;
  }

  if ((header.securityControl & kZigbeeSecurityExtendedNonce) != 0U &&
      !appendLe64(outHeader, 32U, &offset, header.sourceIeee)) {
    return false;
  }

  if ((header.securityControl & 0x18U) == kZigbeeSecurityKeyIdNetwork &&
      !appendBytes(outHeader, 32U, &offset, &header.keySequence, 1U)) {
    return false;
  }

  *outHeaderLength = offset;
  return true;
}

bool ZigbeeSecurity::parseNwkSecurityHeader(const uint8_t* data, uint8_t length,
                                            ZigbeeNwkSecurityHeader* outHeader,
                                            uint8_t* outHeaderLength) {
  if (outHeader != nullptr) {
    memset(outHeader, 0, sizeof(*outHeader));
  }
  if (outHeaderLength != nullptr) {
    *outHeaderLength = 0U;
  }
  if (data == nullptr || outHeader == nullptr || outHeaderLength == nullptr ||
      length < 5U) {
    return false;
  }

  uint8_t offset = 0U;
  outHeader->valid = true;
  outHeader->securityControl = data[offset++];
  outHeader->frameCounter = readLe32(&data[offset]);
  offset = static_cast<uint8_t>(offset + 4U);

  const uint8_t securityLevel = static_cast<uint8_t>(outHeader->securityControl & 0x07U);
  if (securityLevel != kZigbeeSecurityLevelEncMic32) {
    return false;
  }
  outHeader->micLength = kCcmMicLength;

  if ((outHeader->securityControl & kZigbeeSecurityExtendedNonce) != 0U) {
    if (length < static_cast<uint8_t>(offset + 8U)) {
      return false;
    }
    outHeader->sourceIeee = readLe64(&data[offset]);
    offset = static_cast<uint8_t>(offset + 8U);
  }

  if ((outHeader->securityControl & 0x18U) == kZigbeeSecurityKeyIdNetwork) {
    if (length < static_cast<uint8_t>(offset + 1U)) {
      return false;
    }
    outHeader->keySequence = data[offset++];
  }

  *outHeaderLength = offset;
  return true;
}

bool ZigbeeSecurity::buildApsSecurityHeader(
    const ZigbeeApsSecurityHeader& header, uint8_t* outHeader,
    uint8_t* outHeaderLength) {
  if (outHeader == nullptr || outHeaderLength == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outHeader, 32U, &offset, &header.securityControl, 1U) ||
      !appendLe32(outHeader, 32U, &offset, header.frameCounter)) {
    return false;
  }

  if ((header.securityControl & kZigbeeSecurityExtendedNonce) != 0U &&
      !appendLe64(outHeader, 32U, &offset, header.sourceIeee)) {
    return false;
  }

  *outHeaderLength = offset;
  return true;
}

bool ZigbeeSecurity::parseApsSecurityHeader(const uint8_t* data, uint8_t length,
                                            ZigbeeApsSecurityHeader* outHeader,
                                            uint8_t* outHeaderLength) {
  if (outHeader != nullptr) {
    memset(outHeader, 0, sizeof(*outHeader));
  }
  if (outHeaderLength != nullptr) {
    *outHeaderLength = 0U;
  }
  if (data == nullptr || outHeader == nullptr || outHeaderLength == nullptr ||
      length < 5U) {
    return false;
  }

  uint8_t offset = 0U;
  outHeader->valid = true;
  outHeader->securityControl = data[offset++];
  outHeader->frameCounter = readLe32(&data[offset]);
  offset = static_cast<uint8_t>(offset + 4U);

  const uint8_t securityLevel =
      static_cast<uint8_t>(outHeader->securityControl & 0x07U);
  if (securityLevel != kZigbeeSecurityLevelEncMic32) {
    return false;
  }
  outHeader->micLength = kCcmMicLength;

  if ((outHeader->securityControl & 0x18U) != kZigbeeSecurityKeyIdData) {
    return false;
  }

  if ((outHeader->securityControl & kZigbeeSecurityExtendedNonce) != 0U) {
    if (length < static_cast<uint8_t>(offset + 8U)) {
      return false;
    }
    outHeader->sourceIeee = readLe64(&data[offset]);
    offset = static_cast<uint8_t>(offset + 8U);
  }

  *outHeaderLength = offset;
  return true;
}

bool ZigbeeSecurity::encryptCcmStar(const uint8_t key[16],
                                    const uint8_t nonce[13],
                                    const uint8_t* aad, uint8_t aadLength,
                                    const uint8_t* plaintext,
                                    uint8_t plaintextLength,
                                    uint8_t* outCiphertextWithMic,
                                    uint8_t* outCiphertextWithMicLength) {
  if (key == nullptr || nonce == nullptr || outCiphertextWithMic == nullptr ||
      outCiphertextWithMicLength == nullptr) {
    return false;
  }
  if (aadLength > 0U && aad == nullptr) {
    return false;
  }
  if (plaintextLength > 0U && plaintext == nullptr) {
    return false;
  }

  uint8_t offset = 0U;
  uint16_t counter = 1U;
  while (offset < plaintextLength) {
    uint8_t stream[16] = {0U};
    uint8_t ctrBlock[16] = {0U};
    buildCtrBlock(nonce, counter, ctrBlock);
    if (!aesEncryptBlock(key, ctrBlock, stream)) {
      return false;
    }

    const uint8_t remaining = static_cast<uint8_t>(plaintextLength - offset);
    const uint8_t chunk = (remaining < 16U) ? remaining : 16U;
    for (uint8_t i = 0U; i < chunk; ++i) {
      outCiphertextWithMic[offset + i] =
          static_cast<uint8_t>(plaintext[offset + i] ^ stream[i]);
    }
    offset = static_cast<uint8_t>(offset + chunk);
    ++counter;
  }

  uint8_t mic[kCcmMicLength] = {0U};
  if (!computeCcmMic(key, nonce, aad, aadLength, plaintext, plaintextLength,
                     mic)) {
    return false;
  }
  memcpy(&outCiphertextWithMic[plaintextLength], mic, sizeof(mic));
  *outCiphertextWithMicLength =
      static_cast<uint8_t>(plaintextLength + kCcmMicLength);
  return true;
}

bool ZigbeeSecurity::decryptCcmStar(const uint8_t key[16],
                                    const uint8_t nonce[13],
                                    const uint8_t* aad, uint8_t aadLength,
                                    const uint8_t* ciphertextWithMic,
                                    uint8_t ciphertextWithMicLength,
                                    uint8_t* outPlaintext,
                                    uint8_t* outPlaintextLength) {
  if (key == nullptr || nonce == nullptr || ciphertextWithMic == nullptr ||
      outPlaintext == nullptr || outPlaintextLength == nullptr) {
    return false;
  }
  if (aadLength > 0U && aad == nullptr) {
    return false;
  }
  if (ciphertextWithMicLength < kCcmMicLength) {
    return false;
  }

  const uint8_t payloadLength =
      static_cast<uint8_t>(ciphertextWithMicLength - kCcmMicLength);
  uint8_t offset = 0U;
  uint16_t counter = 1U;
  while (offset < payloadLength) {
    uint8_t stream[16] = {0U};
    uint8_t ctrBlock[16] = {0U};
    buildCtrBlock(nonce, counter, ctrBlock);
    if (!aesEncryptBlock(key, ctrBlock, stream)) {
      return false;
    }

    const uint8_t remaining = static_cast<uint8_t>(payloadLength - offset);
    const uint8_t chunk = (remaining < 16U) ? remaining : 16U;
    for (uint8_t i = 0U; i < chunk; ++i) {
      outPlaintext[offset + i] =
          static_cast<uint8_t>(ciphertextWithMic[offset + i] ^ stream[i]);
    }
    offset = static_cast<uint8_t>(offset + chunk);
    ++counter;
  }

  uint8_t expectedMic[kCcmMicLength] = {0U};
  if (!computeCcmMic(key, nonce, aad, aadLength, outPlaintext, payloadLength,
                     expectedMic)) {
    return false;
  }

  uint8_t diff = 0U;
  for (uint8_t i = 0U; i < kCcmMicLength; ++i) {
    diff |= static_cast<uint8_t>(expectedMic[i] ^
                                 ciphertextWithMic[payloadLength + i]);
  }
  if (diff != 0U) {
    return false;
  }

  *outPlaintextLength = payloadLength;
  return true;
}

bool ZigbeeSecurity::buildSecuredNwkFrame(const ZigbeeNetworkFrame& frame,
                                          const ZigbeeNwkSecurityHeader& security,
                                          const uint8_t key[16],
                                          const uint8_t* payload,
                                          uint8_t payloadLength,
                                          uint8_t* outFrame,
                                          uint8_t* outLength) {
  if (key == nullptr || outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (frame.multicast || frame.sourceRoute) {
    return false;
  }

  ZigbeeNetworkFrame headerFrame = frame;
  headerFrame.securityEnabled = false;

  uint8_t nwkHeader[32] = {0U};
  uint8_t nwkHeaderLength = 0U;
  if (!ZigbeeCodec::buildNwkFrame(headerFrame, nullptr, 0U, nwkHeader,
                                  &nwkHeaderLength)) {
    return false;
  }

  uint16_t control = readLe16(nwkHeader);
  control |= (1U << 9U);
  writeLe16(nwkHeader, control);

  uint8_t securityHeader[32] = {0U};
  uint8_t securityHeaderLength = 0U;
  if (!buildNwkSecurityHeader(security, securityHeader, &securityHeaderLength)) {
    return false;
  }

  uint8_t aad[64] = {0U};
  uint8_t aadLength = 0U;
  if (!appendBytes(aad, sizeof(aad), &aadLength, nwkHeader, nwkHeaderLength) ||
      !appendBytes(aad, sizeof(aad), &aadLength, securityHeader,
                   securityHeaderLength)) {
    return false;
  }

  uint8_t nonce[kCcmNonceLength] = {0U};
  if (!buildNwkNonce(security.sourceIeee, security.frameCounter,
                     security.securityControl, nonce)) {
    return false;
  }

  uint8_t ciphertext[127] = {0U};
  uint8_t ciphertextLength = 0U;
  if (!encryptCcmStar(key, nonce, aad, aadLength, payload, payloadLength,
                      ciphertext, &ciphertextLength)) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outFrame, 127U, &offset, nwkHeader, nwkHeaderLength) ||
      !appendBytes(outFrame, 127U, &offset, securityHeader,
                   securityHeaderLength) ||
      !appendBytes(outFrame, 127U, &offset, ciphertext, ciphertextLength)) {
    return false;
  }
  *outLength = offset;
  return true;
}

bool ZigbeeSecurity::parseSecuredNwkFrame(const uint8_t* frame, uint8_t length,
                                          const uint8_t key[16],
                                          ZigbeeNetworkFrame* outFrame,
                                          ZigbeeNwkSecurityHeader* outSecurity,
                                          uint8_t* outPayload,
                                          uint8_t* outPayloadLength) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (outSecurity != nullptr) {
    memset(outSecurity, 0, sizeof(*outSecurity));
  }
  if (outPayloadLength != nullptr) {
    *outPayloadLength = 0U;
  }
  if (frame == nullptr || key == nullptr || outFrame == nullptr ||
      outSecurity == nullptr || outPayload == nullptr ||
      outPayloadLength == nullptr || length < 8U) {
    return false;
  }

  const uint16_t control = readLe16(frame);
  const uint8_t protocolVersion =
      static_cast<uint8_t>((control >> 2U) & 0x0FU);
  if (protocolVersion != kZigbeeNwkProtocolVersion) {
    return false;
  }

  const bool securityEnabled = ((control >> 9U) & 0x1U) != 0U;
  const bool sourceRoute = ((control >> 10U) & 0x1U) != 0U;
  const bool extendedDestination = ((control >> 11U) & 0x1U) != 0U;
  const bool extendedSource = ((control >> 12U) & 0x1U) != 0U;
  const bool multicast = ((control >> 8U) & 0x1U) != 0U;
  if (!securityEnabled || sourceRoute || multicast) {
    return false;
  }

  uint8_t offset = 2U;
  outFrame->valid = true;
  outFrame->frameType = static_cast<ZigbeeNwkFrameType>(control & 0x03U);
  outFrame->discoverRoute = static_cast<uint8_t>((control >> 6U) & 0x03U);
  outFrame->multicast = multicast;
  outFrame->securityEnabled = securityEnabled;
  outFrame->sourceRoute = sourceRoute;
  outFrame->extendedDestination = extendedDestination;
  outFrame->extendedSource = extendedSource;

  if (length < static_cast<uint8_t>(offset + 6U)) {
    return false;
  }
  outFrame->destinationShort = readLe16(&frame[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outFrame->sourceShort = readLe16(&frame[offset]);
  offset = static_cast<uint8_t>(offset + 2U);
  outFrame->radius = frame[offset++];
  outFrame->sequence = frame[offset++];

  if (extendedDestination) {
    if (length < static_cast<uint8_t>(offset + 8U)) {
      return false;
    }
    outFrame->destinationExtended = readLe64(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 8U);
  }
  if (extendedSource) {
    if (length < static_cast<uint8_t>(offset + 8U)) {
      return false;
    }
    outFrame->sourceExtended = readLe64(&frame[offset]);
    offset = static_cast<uint8_t>(offset + 8U);
  }

  uint8_t securityHeaderLength = 0U;
  if (!parseNwkSecurityHeader(&frame[offset],
                              static_cast<uint8_t>(length - offset),
                              outSecurity, &securityHeaderLength)) {
    return false;
  }

  uint8_t aad[64] = {0U};
  uint8_t aadLength = 0U;
  if (!appendBytes(aad, sizeof(aad), &aadLength, frame, offset) ||
      !appendBytes(aad, sizeof(aad), &aadLength, &frame[offset],
                   securityHeaderLength)) {
    return false;
  }
  offset = static_cast<uint8_t>(offset + securityHeaderLength);
  if (length <= offset) {
    return false;
  }

  uint8_t nonce[kCcmNonceLength] = {0U};
  if (!buildNwkNonce(outSecurity->sourceIeee, outSecurity->frameCounter,
                     outSecurity->securityControl, nonce)) {
    return false;
  }

  const uint8_t ciphertextLength = static_cast<uint8_t>(length - offset);
  if (!decryptCcmStar(key, nonce, aad, aadLength, &frame[offset],
                      ciphertextLength, outPayload, outPayloadLength)) {
    return false;
  }

  outFrame->payload = outPayload;
  outFrame->payloadLength = *outPayloadLength;
  return true;
}

bool ZigbeeSecurity::buildSecuredApsCommandFrame(
    const ZigbeeApsCommandFrame& frame, const ZigbeeApsSecurityHeader& security,
    const uint8_t key[16], const uint8_t* payload, uint8_t payloadLength,
    uint8_t* outFrame, uint8_t* outLength) {
  if (key == nullptr || outFrame == nullptr || outLength == nullptr) {
    return false;
  }
  if (payloadLength > 0U && payload == nullptr) {
    return false;
  }
  if (frame.frameType != ZigbeeApsFrameType::kCommand ||
      frame.deliveryMode == kZigbeeApsDeliveryGroup) {
    return false;
  }

  uint8_t header[4] = {0U};
  uint8_t headerLength = 0U;
  uint8_t control = static_cast<uint8_t>(frame.frameType);
  control |= static_cast<uint8_t>((frame.deliveryMode & 0x03U) << 2U);
  control |= (1U << 5U);
  control |= frame.ackRequested ? (1U << 6U) : 0U;
  if (!appendBytes(header, sizeof(header), &headerLength, &control, 1U) ||
      !appendBytes(header, sizeof(header), &headerLength, &frame.counter, 1U)) {
    return false;
  }

  uint8_t securityHeader[32] = {0U};
  uint8_t securityHeaderLength = 0U;
  if (!buildApsSecurityHeader(security, securityHeader, &securityHeaderLength)) {
    return false;
  }

  uint8_t plaintext[127] = {0U};
  uint8_t plaintextLength = 0U;
  if (!appendBytes(plaintext, sizeof(plaintext), &plaintextLength,
                   &frame.commandId, 1U) ||
      !appendBytes(plaintext, sizeof(plaintext), &plaintextLength, payload,
                   payloadLength)) {
    return false;
  }

  uint8_t aad[64] = {0U};
  uint8_t aadLength = 0U;
  if (!appendBytes(aad, sizeof(aad), &aadLength, header, headerLength) ||
      !appendBytes(aad, sizeof(aad), &aadLength, securityHeader,
                   securityHeaderLength)) {
    return false;
  }

  uint8_t nonce[kCcmNonceLength] = {0U};
  if (!buildNwkNonce(security.sourceIeee, security.frameCounter,
                     security.securityControl, nonce)) {
    return false;
  }

  uint8_t ciphertext[127] = {0U};
  uint8_t ciphertextLength = 0U;
  if (!encryptCcmStar(key, nonce, aad, aadLength, plaintext, plaintextLength,
                      ciphertext, &ciphertextLength)) {
    return false;
  }

  uint8_t offset = 0U;
  if (!appendBytes(outFrame, 127U, &offset, header, headerLength) ||
      !appendBytes(outFrame, 127U, &offset, securityHeader,
                   securityHeaderLength) ||
      !appendBytes(outFrame, 127U, &offset, ciphertext, ciphertextLength)) {
    return false;
  }
  *outLength = offset;
  return true;
}

bool ZigbeeSecurity::parseSecuredApsCommandFrame(
    const uint8_t* frame, uint8_t length, const uint8_t key[16],
    ZigbeeApsCommandFrame* outFrame, ZigbeeApsSecurityHeader* outSecurity,
    uint8_t* outPayload, uint8_t* outPayloadLength) {
  if (outFrame != nullptr) {
    memset(outFrame, 0, sizeof(*outFrame));
  }
  if (outSecurity != nullptr) {
    memset(outSecurity, 0, sizeof(*outSecurity));
  }
  if (outPayloadLength != nullptr) {
    *outPayloadLength = 0U;
  }
  if (frame == nullptr || key == nullptr || outFrame == nullptr ||
      outSecurity == nullptr || outPayload == nullptr ||
      outPayloadLength == nullptr || length < 8U) {
    return false;
  }

  const uint8_t control = frame[0];
  const ZigbeeApsFrameType frameType =
      static_cast<ZigbeeApsFrameType>(control & 0x03U);
  const uint8_t deliveryMode = static_cast<uint8_t>((control >> 2U) & 0x03U);
  const bool securityEnabled = ((control >> 5U) & 0x1U) != 0U;
  const bool extendedHeader = ((control >> 7U) & 0x1U) != 0U;
  if (frameType != ZigbeeApsFrameType::kCommand || !securityEnabled ||
      deliveryMode == kZigbeeApsDeliveryGroup || extendedHeader) {
    return false;
  }

  uint8_t offset = 2U;
  outFrame->valid = true;
  outFrame->frameType = frameType;
  outFrame->deliveryMode = deliveryMode;
  outFrame->securityEnabled = securityEnabled;
  outFrame->ackRequested = ((control >> 6U) & 0x1U) != 0U;
  outFrame->counter = frame[1];

  uint8_t securityHeaderLength = 0U;
  if (!parseApsSecurityHeader(&frame[offset],
                              static_cast<uint8_t>(length - offset),
                              outSecurity, &securityHeaderLength)) {
    return false;
  }

  uint8_t aad[64] = {0U};
  uint8_t aadLength = 0U;
  if (!appendBytes(aad, sizeof(aad), &aadLength, frame, offset) ||
      !appendBytes(aad, sizeof(aad), &aadLength, &frame[offset],
                   securityHeaderLength)) {
    return false;
  }
  offset = static_cast<uint8_t>(offset + securityHeaderLength);
  if (length <= offset) {
    return false;
  }

  uint8_t nonce[kCcmNonceLength] = {0U};
  if (!buildNwkNonce(outSecurity->sourceIeee, outSecurity->frameCounter,
                     outSecurity->securityControl, nonce)) {
    return false;
  }

  uint8_t plaintext[127] = {0U};
  uint8_t plaintextLength = 0U;
  if (!decryptCcmStar(key, nonce, aad, aadLength, &frame[offset],
                      static_cast<uint8_t>(length - offset), plaintext,
                      &plaintextLength) ||
      plaintextLength < 1U) {
    return false;
  }

  outFrame->commandId = plaintext[0];
  *outPayloadLength = static_cast<uint8_t>(plaintextLength - 1U);
  if (*outPayloadLength > 0U) {
    memcpy(outPayload, &plaintext[1], *outPayloadLength);
  }
  outFrame->payload = outPayload;
  outFrame->payloadLength = *outPayloadLength;
  return true;
}

bool ZigbeeSecurity::buildSecuredApsTransportKeyCommand(
    const ZigbeeApsTransportKey& transportKey,
    const ZigbeeApsSecurityHeader& security, const uint8_t key[16],
    uint8_t counter, uint8_t* outFrame, uint8_t* outLength) {
  if (outFrame == nullptr || outLength == nullptr) {
    return false;
  }

  uint8_t payload[64] = {0U};
  uint8_t payloadLength = 0U;
  if (!appendBytes(payload, sizeof(payload), &payloadLength,
                   &transportKey.keyType, 1U) ||
      !appendBytes(payload, sizeof(payload), &payloadLength, transportKey.key,
                   sizeof(transportKey.key)) ||
      !appendBytes(payload, sizeof(payload), &payloadLength,
                   &transportKey.keySequence, 1U) ||
      !appendLe64(payload, sizeof(payload), &payloadLength,
                  transportKey.destinationIeee) ||
      !appendLe64(payload, sizeof(payload), &payloadLength,
                  transportKey.sourceIeee)) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  command.frameType = ZigbeeApsFrameType::kCommand;
  command.deliveryMode = kZigbeeApsDeliveryUnicast;
  command.counter = counter;
  command.commandId = kZigbeeApsCommandTransportKey;
  return buildSecuredApsCommandFrame(command, security, key, payload,
                                     payloadLength, outFrame, outLength);
}

bool ZigbeeSecurity::parseSecuredApsTransportKeyCommand(
    const uint8_t* frame, uint8_t length, const uint8_t key[16],
    ZigbeeApsTransportKey* outTransportKey, ZigbeeApsSecurityHeader* outSecurity,
    uint8_t* outCounter) {
  if (outTransportKey != nullptr) {
    memset(outTransportKey, 0, sizeof(*outTransportKey));
  }
  if (outCounter != nullptr) {
    *outCounter = 0U;
  }
  if (frame == nullptr || key == nullptr || outTransportKey == nullptr ||
      outSecurity == nullptr || outCounter == nullptr) {
    return false;
  }

  ZigbeeApsCommandFrame command{};
  uint8_t payload[64] = {0U};
  uint8_t payloadLength = 0U;
  if (!parseSecuredApsCommandFrame(frame, length, key, &command, outSecurity,
                                   payload, &payloadLength) ||
      !command.valid ||
      command.commandId != kZigbeeApsCommandTransportKey ||
      payloadLength != 33U) {
    return false;
  }

  uint8_t offset = 0U;
  outTransportKey->valid = true;
  outTransportKey->keyType = payload[offset++];
  if (outTransportKey->keyType != kZigbeeApsTransportKeyStandardNetworkKey) {
    return false;
  }
  memcpy(outTransportKey->key, &payload[offset], sizeof(outTransportKey->key));
  offset = static_cast<uint8_t>(offset + sizeof(outTransportKey->key));
  outTransportKey->keySequence = payload[offset++];
  outTransportKey->destinationIeee = readLe64(&payload[offset]);
  offset = static_cast<uint8_t>(offset + 8U);
  outTransportKey->sourceIeee = readLe64(&payload[offset]);
  *outCounter = command.counter;
  return true;
}

}  // namespace xiao_nrf54l15
