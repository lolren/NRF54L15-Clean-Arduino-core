#include "matter_manual_pairing.h"

#include <stdio.h>
#include <string.h>

namespace xiao_nrf54l15 {
namespace {

constexpr uint32_t kSetupPinMaximumValue = 99999998UL;
constexpr uint16_t kDiscriminatorMaximumValue = 0x0FFFU;

constexpr uint8_t kVerhoeffD[10][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    {1, 2, 3, 4, 0, 6, 7, 8, 9, 5},
    {2, 3, 4, 0, 1, 7, 8, 9, 5, 6},
    {3, 4, 0, 1, 2, 8, 9, 5, 6, 7},
    {4, 0, 1, 2, 3, 9, 5, 6, 7, 8},
    {5, 9, 8, 7, 6, 0, 4, 3, 2, 1},
    {6, 5, 9, 8, 7, 1, 0, 4, 3, 2},
    {7, 6, 5, 9, 8, 2, 1, 0, 4, 3},
    {8, 7, 6, 5, 9, 3, 2, 1, 0, 4},
    {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
};

constexpr uint8_t kVerhoeffP[8][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    {1, 5, 7, 6, 2, 8, 3, 0, 9, 4},
    {5, 8, 0, 3, 7, 9, 6, 1, 4, 2},
    {8, 9, 1, 6, 0, 4, 3, 5, 2, 7},
    {9, 4, 5, 3, 1, 2, 6, 8, 7, 0},
    {4, 2, 8, 6, 5, 7, 3, 9, 0, 1},
    {2, 7, 9, 3, 8, 0, 6, 4, 1, 5},
    {7, 0, 4, 6, 9, 1, 3, 2, 5, 8},
};

constexpr uint8_t kVerhoeffInv[10] = {0, 4, 3, 2, 1, 5, 6, 7, 8, 9};

bool hasNonStandardCommissioningFlow(MatterCommissioningFlow flow) {
  return flow != MatterCommissioningFlow::kStandard;
}

char computeVerhoeffCheckChar(const char* digits) {
  uint8_t checksum = 0;
  const size_t len = strlen(digits);
  for (size_t i = 0; i < len; ++i) {
    const char ch = digits[len - 1U - i];
    if (ch < '0' || ch > '9') {
      return '\0';
    }
    checksum = kVerhoeffD[checksum][kVerhoeffP[(i + 1U) % 8U][ch - '0']];
  }
  return static_cast<char>('0' + kVerhoeffInv[checksum]);
}

}  // namespace

bool matterSetupPinValid(uint32_t setupPinCode) {
  if (setupPinCode == 0U || setupPinCode > kSetupPinMaximumValue) {
    return false;
  }

  switch (setupPinCode) {
    case 11111111UL:
    case 22222222UL:
    case 33333333UL:
    case 44444444UL:
    case 55555555UL:
    case 66666666UL:
    case 77777777UL:
    case 88888888UL:
    case 12345678UL:
    case 87654321UL:
      return false;
    default:
      return true;
  }
}

bool matterDiscriminatorValid(uint16_t discriminator) {
  return discriminator <= kDiscriminatorMaximumValue;
}

bool matterManualPairingPayloadValid(const MatterManualPairingPayload& payload) {
  if (!matterSetupPinValid(payload.setupPinCode) ||
      !matterDiscriminatorValid(payload.discriminator)) {
    return false;
  }

  switch (payload.commissioningFlow) {
    case MatterCommissioningFlow::kStandard:
    case MatterCommissioningFlow::kUserActionRequired:
    case MatterCommissioningFlow::kCustom:
      break;
    default:
      return false;
  }

  if (payload.vendorId != 0U && payload.productId == 0U) {
    return false;
  }

  return true;
}

size_t matterManualPairingCodeLength(const MatterManualPairingPayload& payload) {
  return hasNonStandardCommissioningFlow(payload.commissioningFlow)
             ? kMatterManualPairingLongCodeLength
             : kMatterManualPairingShortCodeLength;
}

bool matterManualPairingCode(const MatterManualPairingPayload& payload,
                             char* outBuffer, size_t outBufferSize) {
  if (outBuffer == nullptr || !matterManualPairingPayloadValid(payload)) {
    return false;
  }

  const bool useLongCode =
      hasNonStandardCommissioningFlow(payload.commissioningFlow);
  const size_t codeLength = matterManualPairingCodeLength(payload);
  if (outBufferSize < (codeLength + 1U)) {
    return false;
  }

  const uint8_t shortDiscriminator =
      static_cast<uint8_t>(payload.discriminator >> 8U);
  const uint32_t chunk1 =
      ((shortDiscriminator >> 2U) & 0x03U) | (useLongCode ? 0x04U : 0x00U);
  const uint32_t chunk2 =
      (payload.setupPinCode & 0x3FFFUL) |
      ((static_cast<uint32_t>(shortDiscriminator) & 0x03UL) << 14U);
  const uint32_t chunk3 = (payload.setupPinCode >> 14U) & 0x1FFFUL;

  char withoutCheck[kMatterManualPairingLongCodeLength] = {0};
  const int written =
      useLongCode
          ? snprintf(withoutCheck, sizeof(withoutCheck), "%1lu%05lu%04lu%05u%05u",
                     static_cast<unsigned long>(chunk1),
                     static_cast<unsigned long>(chunk2),
                     static_cast<unsigned long>(chunk3),
                     static_cast<unsigned>(payload.vendorId),
                     static_cast<unsigned>(payload.productId))
          : snprintf(withoutCheck, sizeof(withoutCheck), "%1lu%05lu%04lu",
                     static_cast<unsigned long>(chunk1),
                     static_cast<unsigned long>(chunk2),
                     static_cast<unsigned long>(chunk3));
  if (written != static_cast<int>(codeLength - 1U)) {
    return false;
  }

  const char checkChar = computeVerhoeffCheckChar(withoutCheck);
  if (checkChar == '\0') {
    return false;
  }

  memcpy(outBuffer, withoutCheck, codeLength - 1U);
  outBuffer[codeLength - 1U] = checkChar;
  outBuffer[codeLength] = '\0';
  return true;
}

}  // namespace xiao_nrf54l15
