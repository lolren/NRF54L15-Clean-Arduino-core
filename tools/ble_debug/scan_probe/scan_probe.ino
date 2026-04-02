#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;

static const uint8_t kTargetAddress[6] = {0x37, 0x00, 0x15, 0x54, 0xDE, 0xC0};
static const char kTargetName[] = "X54-LB";
static const uint8_t kUartServiceUuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5,
                                             0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5,
                                             0x01, 0x00, 0x40, 0x6E};

volatile uint32_t g_hitCount = 0U;
volatile uint32_t g_matchCount = 0U;
volatile uint32_t g_uuidMatchCount = 0U;
volatile uint32_t g_nameMatchCount = 0U;
volatile uint8_t g_lastAddress[6] = {0U};
volatile uint8_t g_matchAddress[6] = {0U};
volatile uint8_t g_lastHeader = 0U;
volatile uint8_t g_lastPayloadLength = 0U;
volatile int8_t g_lastRssi = 0;
volatile uint8_t g_matchHeader = 0U;
volatile uint8_t g_matchPayloadLength = 0U;
volatile int8_t g_matchRssi = 0;
volatile uint8_t g_matchRandom = 0U;
volatile uint8_t g_matchScanRsp = 0U;

static bool addressEquals(const uint8_t* lhs, const uint8_t* rhs) {
  return lhs != nullptr && rhs != nullptr && memcmp(lhs, rhs, 6U) == 0;
}

static bool payloadHasName(const uint8_t* payload, uint8_t length, const char* name) {
  if (payload == nullptr || name == nullptr) {
    return false;
  }
  const size_t nameLength = strlen(name);
  uint8_t offset = 0U;
  while (offset < length) {
    const uint8_t fieldLength = payload[offset];
    if (fieldLength == 0U ||
        static_cast<uint16_t>(offset + fieldLength) >=
            static_cast<uint16_t>(length + 1U)) {
      break;
    }
    const uint8_t type = payload[offset + 1U];
    if ((type == 0x08U || type == 0x09U) &&
        static_cast<uint8_t>(fieldLength - 1U) == nameLength &&
        memcmp(&payload[offset + 2U], name, nameLength) == 0) {
      return true;
    }
    offset = static_cast<uint8_t>(offset + fieldLength + 1U);
  }
  return false;
}

static bool payloadHasUuid128(const uint8_t* payload, uint8_t length,
                              const uint8_t uuid128[16]) {
  if (payload == nullptr || uuid128 == nullptr) {
    return false;
  }
  uint8_t offset = 0U;
  while (offset < length) {
    const uint8_t fieldLength = payload[offset];
    if (fieldLength == 0U ||
        static_cast<uint16_t>(offset + fieldLength) >=
            static_cast<uint16_t>(length + 1U)) {
      break;
    }
    const uint8_t type = payload[offset + 1U];
    if ((type == 0x06U || type == 0x07U) && fieldLength >= 17U) {
      const uint8_t valueLength = static_cast<uint8_t>(fieldLength - 1U);
      for (uint8_t pos = 0U; (pos + 16U) <= valueLength;
           pos = static_cast<uint8_t>(pos + 16U)) {
        if (memcmp(&payload[offset + 2U + pos], uuid128, 16U) == 0) {
          return true;
        }
      }
    }
    offset = static_cast<uint8_t>(offset + fieldLength + 1U);
  }
  return false;
}

void setup() {
  g_ble.begin(-4);
  static const uint8_t kAddress[6] = {0x44, 0x00, 0x15, 0x54, 0xDE, 0xC0};
  g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic);
}

void loop() {
  BleActiveScanResult scan{};
  if (!g_ble.scanActiveCycle(&scan, 900000UL, 300000UL)) {
    delay(1);
    return;
  }

  ++g_hitCount;
  memcpy(const_cast<uint8_t*>(g_lastAddress), scan.advertiserAddress, 6U);
  g_lastHeader = scan.advHeader;
  g_lastPayloadLength = scan.advPayloadLength;
  g_lastRssi = scan.advRssiDbm;

  const bool addressMatch = addressEquals(scan.advertiserAddress, kTargetAddress);
  const bool uuidMatch = payloadHasUuid128(scan.advPayload, scan.advPayloadLength,
                                           kUartServiceUuid);
  const bool nameMatch = scan.scanResponseReceived &&
                         payloadHasName(scan.scanRspPayload, scan.scanRspPayloadLength,
                                        kTargetName);

  if (uuidMatch) {
    ++g_uuidMatchCount;
  }
  if (nameMatch) {
    ++g_nameMatchCount;
  }

  if (addressMatch || uuidMatch || nameMatch) {
    ++g_matchCount;
    memcpy(const_cast<uint8_t*>(g_matchAddress), scan.advertiserAddress, 6U);
    g_matchHeader = scan.advHeader;
    g_matchPayloadLength = scan.advPayloadLength;
    g_matchRssi = scan.advRssiDbm;
    g_matchRandom = scan.advertiserAddressRandom ? 1U : 0U;
    g_matchScanRsp = scan.scanResponseReceived ? 1U : 0U;
  }
}
