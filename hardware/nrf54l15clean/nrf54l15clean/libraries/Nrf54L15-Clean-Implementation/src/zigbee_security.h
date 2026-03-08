#pragma once

#include <stdint.h>

#include "zigbee_stack.h"

namespace xiao_nrf54l15 {

constexpr uint8_t kZigbeeSecurityLevelEncMic32 = 0x05U;
constexpr uint8_t kZigbeeSecurityKeyIdNetwork = 0x08U;
constexpr uint8_t kZigbeeSecurityExtendedNonce = 0x20U;
constexpr uint8_t kZigbeeSecurityControlNwkEncMic32 =
    static_cast<uint8_t>(kZigbeeSecurityLevelEncMic32 |
                         kZigbeeSecurityKeyIdNetwork |
                         kZigbeeSecurityExtendedNonce);

struct ZigbeeNwkSecurityHeader {
  bool valid = false;
  uint8_t securityControl = kZigbeeSecurityControlNwkEncMic32;
  uint32_t frameCounter = 0U;
  uint64_t sourceIeee = 0U;
  uint8_t keySequence = 0U;
  uint8_t micLength = 4U;
};

class ZigbeeSecurity {
 public:
  static bool loadZigbeeAlliance09LinkKey(uint8_t outKey[16]);

  static bool buildNwkNonce(uint64_t sourceIeee, uint32_t frameCounter,
                            uint8_t securityControl, uint8_t outNonce[13]);
  static bool buildNwkSecurityHeader(const ZigbeeNwkSecurityHeader& header,
                                     uint8_t* outHeader,
                                     uint8_t* outHeaderLength);
  static bool parseNwkSecurityHeader(const uint8_t* data, uint8_t length,
                                     ZigbeeNwkSecurityHeader* outHeader,
                                     uint8_t* outHeaderLength);

  static bool encryptCcmStar(const uint8_t key[16], const uint8_t nonce[13],
                             const uint8_t* aad, uint8_t aadLength,
                             const uint8_t* plaintext, uint8_t plaintextLength,
                             uint8_t* outCiphertextWithMic,
                             uint8_t* outCiphertextWithMicLength);
  static bool decryptCcmStar(const uint8_t key[16], const uint8_t nonce[13],
                             const uint8_t* aad, uint8_t aadLength,
                             const uint8_t* ciphertextWithMic,
                             uint8_t ciphertextWithMicLength,
                             uint8_t* outPlaintext,
                             uint8_t* outPlaintextLength);

  static bool buildSecuredNwkFrame(const ZigbeeNetworkFrame& frame,
                                   const ZigbeeNwkSecurityHeader& security,
                                   const uint8_t key[16],
                                   const uint8_t* payload,
                                   uint8_t payloadLength, uint8_t* outFrame,
                                   uint8_t* outLength);
  static bool parseSecuredNwkFrame(const uint8_t* frame, uint8_t length,
                                   const uint8_t key[16],
                                   ZigbeeNetworkFrame* outFrame,
                                   ZigbeeNwkSecurityHeader* outSecurity,
                                   uint8_t* outPayload,
                                   uint8_t* outPayloadLength);
};

}  // namespace xiao_nrf54l15
