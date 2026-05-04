#pragma once

#include <stddef.h>
#include <stdint.h>

#include "matter_foundation_target.h"
#include "nrf54_thread_experimental.h"

#if defined(NRF54L15_CLEAN_MATTER_CORE_ENABLE) && \
    (NRF54L15_CLEAN_MATTER_CORE_ENABLE != 0)
#include <lib/core/CHIPError.h>
#endif

namespace xiao_nrf54l15 {

// Matter platform initialization and runtime ownership for nRF54L15.
// This is the bridge between the Arduino core and the staged CHIP runtime.

struct MatterPlatformConfig {
  const char* storageNamespace = "matter_plat";
  bool autoStartThread = true;
  bool wipeSettings = false;
  uint16_t udpPort = Nrf54MatterOnOffLightFoundation::kMatterUdpPort;
};

struct MatterPlatformState {
  bool initialized = false;
  bool storageOpen = false;
  bool threadStarted = false;
  bool threadAttached = false;
  bool udpBound = false;
  uint32_t uptimeMs = 0U;
  uint32_t rxCount = 0U;
  uint32_t txCount = 0U;
  uint32_t dropCount = 0U;
  uint32_t lastError = 0U;
  Nrf54ThreadExperimental::Role threadRole =
      Nrf54ThreadExperimental::Role::kUnknown;
};

class MatterPlatform {
 public:
  MatterPlatform() = default;

  bool begin(const MatterPlatformConfig& config = MatterPlatformConfig());
  void end();
  void process();

  bool ready() const;
  bool snapshot(MatterPlatformState* outState) const;
  bool sendUdp(const uint8_t* payload, uint16_t length,
               const otIp6Address& destAddr, uint16_t destPort);
  bool setReceiveCallback(void (*callback)(void* context,
                                           const uint8_t* payload,
                                           uint16_t length,
                                           const otIp6Address& source,
                                           uint16_t sourcePort),
                          void* context = nullptr);
  bool setFactoryData(const uint8_t* data, size_t length);
  bool getFactoryData(uint8_t* outData, size_t maxLength,
                      size_t* outLength = nullptr) const;
  size_t factoryDataLength() const;

  Nrf54ThreadExperimental& thread();
  const Nrf54ThreadExperimental& thread() const;

  uint32_t uptimeMs() const;
  uint32_t getMonotonicMilliseconds() const;

  static bool getUniqueId(uint8_t outId[16]);
  static uint64_t getHardwareUniqueId();
  static void secureZero(void* ptr, size_t length);

 private:
  static void handleUdpReceiveStatic(void* context, const uint8_t* payload,
                                     uint16_t length,
                                     const otMessageInfo& messageInfo);

  MatterPlatformConfig config_ = {};
  Nrf54ThreadExperimental thread_;
  bool storageOpen_ = false;
  bool udpBound_ = false;
  uint32_t rxCount_ = 0U;
  uint32_t txCount_ = 0U;
  uint32_t dropCount_ = 0U;
  uint32_t lastError_ = 0U;
  void (*receiveCallback_)(void* context, const uint8_t* payload,
                           uint16_t length, const otIp6Address& source,
                           uint16_t sourcePort) = nullptr;
  void* receiveContext_ = nullptr;
  uint8_t factoryData_[128] = {0};
  size_t factoryDataLength_ = 0U;
};

inline const char* matterPlatformBuildMode() {
  return MatterRuntimeOwnership::kMatterBuildSeamCurrentEnabled
             ? "staged-platform"
             : "disabled";
}

}  // namespace xiao_nrf54l15
