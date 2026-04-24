#pragma once

#include <stddef.h>
#include <stdint.h>

namespace xiao_nrf54l15 {

enum class MatterCommissioningFlow : uint8_t {
  kStandard = 0,
  kUserActionRequired = 1,
  kCustom = 2,
};

struct MatterManualPairingPayload {
  uint32_t setupPinCode = 0;
  uint16_t discriminator = 0;
  uint16_t vendorId = 0;
  uint16_t productId = 0;
  MatterCommissioningFlow commissioningFlow = MatterCommissioningFlow::kStandard;
};

constexpr size_t kMatterManualPairingShortCodeLength = 11;
constexpr size_t kMatterManualPairingLongCodeLength = 21;

bool matterSetupPinValid(uint32_t setupPinCode);
bool matterDiscriminatorValid(uint16_t discriminator);
bool matterManualPairingPayloadValid(const MatterManualPairingPayload& payload);
size_t matterManualPairingCodeLength(const MatterManualPairingPayload& payload);

bool matterManualPairingCode(const MatterManualPairingPayload& payload,
                             char* outBuffer, size_t outBufferSize);

}  // namespace xiao_nrf54l15
