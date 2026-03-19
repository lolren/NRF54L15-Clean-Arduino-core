#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

// Minimal scannable extended advertising example.
//
// Current scope:
// - primary ADV_EXT_IND on channels 37/38/39
// - one AUX_ADV_IND on a fixed secondary channel
// - one AUX_SCAN_RSP payload on the same secondary channel
// - LE 1M only
// - non-connectable, scannable

static BleRadio g_ble;
static PowerManager g_power;

static uint32_t g_lastLogMs = 0;
static uint32_t g_events = 0;
static uint8_t g_scanRspData[kBleExtendedAdvDataMaxLength];
static size_t g_scanRspDataLen = 0U;

static constexpr int8_t kTxPowerDbm = 0;
static constexpr uint8_t kAdvertisingSid = 6U;
static constexpr uint8_t kAuxChannel = 21U;
static constexpr uint32_t kAuxOffsetUs = 3000UL;
static constexpr uint32_t kInterPrimaryDelayUs = 350UL;
static constexpr uint32_t kRequestListenSpinLimit = 250000UL;
static constexpr uint32_t kSpinLimit = 900000UL;
static constexpr uint32_t kAdvertisingIntervalMs = 120UL;
static constexpr uint16_t kCompanyId = 0x3154U;
static constexpr char kName[] = "X54-EXT-SCAN";
static constexpr uint8_t kAddress[6] = {0x61, 0x00, 0x15, 0x54, 0xDE, 0xC0};

static bool appendAdField(uint8_t type, const uint8_t* value, size_t valueLen) {
  if ((valueLen > 0U) && (value == nullptr)) {
    return false;
  }
  if (valueLen > 254U) {
    return false;
  }
  if ((g_scanRspDataLen + valueLen + 2U) > sizeof(g_scanRspData)) {
    return false;
  }

  g_scanRspData[g_scanRspDataLen++] = static_cast<uint8_t>(valueLen + 1U);
  g_scanRspData[g_scanRspDataLen++] = type;
  if (valueLen > 0U) {
    memcpy(&g_scanRspData[g_scanRspDataLen], value, valueLen);
    g_scanRspDataLen += valueLen;
  }
  return true;
}

static bool buildScanResponsePayload() {
  g_scanRspDataLen = 0U;

  if (!appendAdField(0x09U, reinterpret_cast<const uint8_t*>(kName), strlen(kName))) {
    return false;
  }

  uint8_t manufacturer[220];
  manufacturer[0] = static_cast<uint8_t>(kCompanyId & 0xFFU);
  manufacturer[1] = static_cast<uint8_t>((kCompanyId >> 8U) & 0xFFU);
  for (size_t i = 2U; i < sizeof(manufacturer); ++i) {
    manufacturer[i] = static_cast<uint8_t>('A' + ((i - 2U) % 26U));
  }

  if (!appendAdField(0xFFU, manufacturer, sizeof(manufacturer))) {
    return false;
  }

  return (g_scanRspDataLen > kBleLegacyAdDataMaxLength);
}

static void printAddress(const uint8_t* addr) {
  for (int i = 5; i >= 0; --i) {
    if (addr[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(addr[i], HEX);
    if (i > 0) {
      Serial.print(':');
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(350);

  Serial.print("\r\nBleExtendedScannableAdv251 start\r\n");
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  g_power.setLatencyMode(PowerLatencyMode::kLowPower);

  bool ok = buildScanResponsePayload();
  if (ok) {
    ok = g_ble.begin(kTxPowerDbm);
  }
  if (ok) {
    ok = g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic);
  }
  if (ok) {
    ok = g_ble.setExtendedAdvertisingSid(kAdvertisingSid);
  }
  if (ok) {
    ok = g_ble.setExtendedAdvertisingAuxChannel(kAuxChannel);
  }
  if (ok) {
    ok = g_ble.setExtendedScanResponseData(g_scanRspData, g_scanRspDataLen);
  }

  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
  if (!ok) {
    return;
  }

  Serial.print("addr=");
  printAddress(kAddress);
  Serial.print(" type=random\r\n");
  Serial.print("scan_rsp_len=");
  Serial.print(g_scanRspDataLen);
  Serial.print(" sid=");
  Serial.print(kAdvertisingSid);
  Serial.print(" aux_channel=");
  Serial.print(kAuxChannel);
  Serial.print(" aux_offset_us=");
  Serial.print(kAuxOffsetUs);
  Serial.print("\r\n");
}

void loop() {
  const bool ok = g_ble.advertiseExtendedScannableEvent(
      kAuxOffsetUs, kInterPrimaryDelayUs, kRequestListenSpinLimit, kSpinLimit);
  ++g_events;

  Gpio::write(kPinUserLed, (g_events & 0x1U) == 0U);

  const uint32_t now = millis();
  if ((now - g_lastLogMs) >= 1000UL) {
    g_lastLogMs = now;

    char line[160];
    snprintf(line, sizeof(line),
             "t=%lu ext_scannable_events=%lu last=%s scan_rsp_len=%u sid=%u aux_ch=%u\r\n",
             static_cast<unsigned long>(now),
             static_cast<unsigned long>(g_events),
             ok ? "OK" : "FAIL",
             static_cast<unsigned>(g_scanRspDataLen),
             static_cast<unsigned>(kAdvertisingSid),
             static_cast<unsigned>(kAuxChannel));
    Serial.print(line);
  }

  delay(kAdvertisingIntervalMs);
}
