#include "matter_platform_stage.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace xiao_nrf54l15 {
namespace {

constexpr char kFactoryDataKey[] = "factory_data";

}  // namespace

bool MatterPlatform::begin(const MatterPlatformConfig& config) {
  if (storageOpen_) {
    return false;
  }

  config_ = config;

  if (config_.autoStartThread && !thread_.begin(config_.wipeSettings)) {
    return false;
  }

  storageOpen_ = true;
  return true;
}

void MatterPlatform::end() {
  if (thread_.started()) {
    thread_.stop();
  }
  storageOpen_ = false;
  receiveCallback_ = nullptr;
  receiveContext_ = nullptr;
}

void MatterPlatform::process() {
  thread_.process();
}

bool MatterPlatform::ready() const {
  return storageOpen_ && thread_.started() && thread_.attached();
}

bool MatterPlatform::snapshot(MatterPlatformState* outState) const {
  if (outState == nullptr) {
    return false;
  }

  memset(outState, 0, sizeof(*outState));
  outState->initialized = storageOpen_;
  outState->storageOpen = storageOpen_;
  outState->threadStarted = thread_.started();
  outState->threadAttached = thread_.attached();
  outState->udpBound = udpBound_;
  outState->uptimeMs = millis();
  outState->rxCount = rxCount_;
  outState->txCount = txCount_;
  outState->dropCount = dropCount_;
  outState->lastError = lastError_;
  outState->threadRole = thread_.role();
  return true;
}

bool MatterPlatform::sendUdp(const uint8_t* payload, uint16_t length,
                             const otIp6Address& destAddr, uint16_t destPort) {
  if (payload == nullptr || length == 0U) {
    lastError_ = static_cast<uint32_t>(OT_ERROR_INVALID_ARGS);
    return false;
  }

  if (!udpBound_ || !thread_.udpOpened()) {
    lastError_ = static_cast<uint32_t>(OT_ERROR_INVALID_STATE);
    return false;
  }

  const bool ok = thread_.sendUdp(destAddr, destPort, payload, length);
  if (ok) {
    txCount_++;
  }
  lastError_ = static_cast<uint32_t>(ok ? OT_ERROR_NONE : thread_.lastUdpError());
  return ok;
}

bool MatterPlatform::setReceiveCallback(
    void (*callback)(void* context, const uint8_t* payload, uint16_t length,
                     const otIp6Address& source, uint16_t sourcePort),
    void* context) {
  receiveCallback_ = callback;
  receiveContext_ = context;

  if (callback != nullptr && !udpBound_) {
    const bool opened =
        thread_.openUdp(config_.udpPort, handleUdpReceiveStatic, this);
    if (opened) {
      udpBound_ = true;
    }
    return opened;
  }

  return true;
}

bool MatterPlatform::setFactoryData(const uint8_t* data, size_t length) {
  if (data == nullptr && length != 0U) {
    return false;
  }

  if (length > sizeof(factoryData_)) {
    return false;
  }

  if (length == 0U) {
    factoryDataLength_ = 0U;
    memset(factoryData_, 0, sizeof(factoryData_));
    return true;
  }

  memcpy(factoryData_, data, length);
  factoryDataLength_ = length;
  return true;
}

bool MatterPlatform::getFactoryData(uint8_t* outData, size_t maxLength,
                                    size_t* outLength) const {
  if (outLength != nullptr) {
    *outLength = 0U;
  }

  if (outData == nullptr) {
    return factoryDataLength_ > 0U;
  }

  if (maxLength < factoryDataLength_) {
    return false;
  }

  if (factoryDataLength_ > 0U) {
    memcpy(outData, factoryData_, factoryDataLength_);
  }

  if (outLength != nullptr) {
    *outLength = factoryDataLength_;
  }
  return true;
}

size_t MatterPlatform::factoryDataLength() const {
  return factoryDataLength_;
}

Nrf54ThreadExperimental& MatterPlatform::thread() {
  return thread_;
}

const Nrf54ThreadExperimental& MatterPlatform::thread() const {
  return thread_;
}

uint32_t MatterPlatform::uptimeMs() const {
  return millis();
}

uint32_t MatterPlatform::getMonotonicMilliseconds() const {
  return millis();
}

bool MatterPlatform::getUniqueId(uint8_t outId[16]) {
  if (outId == nullptr) {
    return false;
  }

  // Use FICR INFO device UUID + derivation
  const uint64_t deviceId =
      *reinterpret_cast<const volatile uint32_t*>(0xFFC000A0UL) |
      (static_cast<uint64_t>(
           *reinterpret_cast<const volatile uint32_t*>(0xFFC000A4UL))
       << 32U);

  for (size_t i = 0; i < 8; ++i) {
    outId[i] = static_cast<uint8_t>(deviceId >> (i * 8U));
    outId[i + 8] = static_cast<uint8_t>(
        (deviceId ^ 0x5A3C9E27F4B18D06ULL) >> (i * 8U));
  }
  return true;
}

uint64_t MatterPlatform::getHardwareUniqueId() {
  return *reinterpret_cast<const volatile uint32_t*>(0xFFC000A0UL) |
         (static_cast<uint64_t>(
              *reinterpret_cast<const volatile uint32_t*>(0xFFC000A4UL))
          << 32U);
}

void MatterPlatform::secureZero(void* ptr, size_t length) {
  if (ptr == nullptr) {
    return;
  }
  volatile uint8_t* bytes = static_cast<volatile uint8_t*>(ptr);
  while (length-- > 0U) {
    *bytes++ = 0U;
  }
}

void MatterPlatform::handleUdpReceiveStatic(
    void* context, const uint8_t* payload, uint16_t length,
    const otMessageInfo& messageInfo) {
  if (context == nullptr) {
    return;
  }

  MatterPlatform* platform = static_cast<MatterPlatform*>(context);
  platform->rxCount_++;

  if (platform->receiveCallback_ != nullptr) {
    platform->receiveCallback_(platform->receiveContext_, payload, length,
                               messageInfo.mPeerAddr,
                               messageInfo.mPeerPort);
  } else {
    platform->dropCount_++;
  }
}

}  // namespace xiao_nrf54l15
