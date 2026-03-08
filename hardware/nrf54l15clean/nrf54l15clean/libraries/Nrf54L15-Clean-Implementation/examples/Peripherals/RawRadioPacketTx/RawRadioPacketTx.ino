#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr uint8_t kConfigFrequencyOffsetMhz = 8U;  // 2408 MHz
static constexpr uint32_t kConfigAddressBase0 = 0xC2C2C2C2UL;
static constexpr uint8_t kConfigAddressPrefix0 = 0xC2U;
static constexpr int8_t kConfigTxPowerDbm = -8;
static constexpr uint8_t kConfigPayloadLength = 8U;
static constexpr char kConfigPayloadTag[] = "RAWTX1";

static constexpr uint32_t kTxIntervalMs = 1000U;

RawRadioLink gRadio;
uint8_t gPayload[kConfigPayloadLength] = {0};
uint32_t gCounter = 0U;
uint32_t gLastTxMs = 0U;

void ledOn() {
  (void)Gpio::write(kPinUserLed, false);
}

void ledOff() {
  (void)Gpio::write(kPinUserLed, true);
}

void pulse(uint16_t onMs = 20U, uint16_t offMs = 0U) {
  ledOn();
  delay(onMs);
  ledOff();
  if (offMs > 0U) {
    delay(offMs);
  }
}

[[noreturn]] void failStage(uint8_t stage) {
  while (true) {
    for (uint8_t i = 0; i < stage; ++i) {
      pulse(90U, 120U);
    }
    delay(900U);
  }
}

RawRadioConfig makeRadioConfig() {
  RawRadioConfig config;
  config.frequencyOffsetMhz = kConfigFrequencyOffsetMhz;
  config.addressBase0 = kConfigAddressBase0;
  config.addressPrefix0 = kConfigAddressPrefix0;
  config.txPowerDbm = kConfigTxPowerDbm;
  config.maxPayloadLength = kConfigPayloadLength;
  return config;
}

void configureBoard() {
  (void)Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  ledOff();

  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && (static_cast<uint32_t>(millis() - start) < 1500U)) {
  }

  BoardControl::setBatterySenseEnabled(false);
  BoardControl::setImuMicEnabled(false);
  BoardControl::collapseRfPathIdle();
}

void buildPayload() {
  gPayload[0] = static_cast<uint8_t>(gCounter & 0xFFU);
  gPayload[1] = static_cast<uint8_t>((gCounter >> 8U) & 0xFFU);
  for (uint8_t i = 0; i < (kConfigPayloadLength - 2U); ++i) {
    const char ch = kConfigPayloadTag[i];
    gPayload[2U + i] = (ch != '\0') ? static_cast<uint8_t>(ch)
                                     : static_cast<uint8_t>('.');
  }
}

bool transmitOnce() {
  if (!BoardControl::enableRfPath(BoardAntennaPath::kCeramic)) {
    return false;
  }

  buildPayload();
  const bool ok = gRadio.transmit(gPayload, kConfigPayloadLength);
  BoardControl::collapseRfPathIdle();
  return ok;
}

}  // namespace

void setup() {
  configureBoard();
  if (!gRadio.begin(makeRadioConfig())) {
    failStage(2);
  }

  Serial.println(F("Raw RADIO packet TX"));
  Serial.println(F("Mode: NRF 1Mbit proprietary"));
  Serial.print(F("Pipe: BASE0=0x"));
  Serial.print(kConfigAddressBase0, HEX);
  Serial.print(F(" PREFIX0=0x"));
  Serial.print(kConfigAddressPrefix0, HEX);
  Serial.print(F(" FREQ=24"));
  Serial.print(kConfigFrequencyOffsetMhz);
  Serial.print(F(" MHz TX="));
  Serial.print(static_cast<int>(kConfigTxPowerDbm));
  Serial.println(F(" dBm"));
  pulse(40U, 80U);
}

void loop() {
  const uint32_t now = millis();
  if ((now - gLastTxMs) < kTxIntervalMs) {
    delay(10U);
    return;
  }

  gLastTxMs = now;
  if (!transmitOnce()) {
    failStage(3);
  }

  Serial.print(F("TX counter="));
  Serial.println(gCounter);
  pulse();
  ++gCounter;
}
