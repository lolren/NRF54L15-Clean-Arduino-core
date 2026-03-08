#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr uint8_t kConfigFrequencyOffsetMhz = 8U;  // 2408 MHz
static constexpr uint32_t kConfigAddressBase0 = 0xC2C2C2C2UL;
static constexpr uint8_t kConfigAddressPrefix0 = 0xC2U;
static constexpr int8_t kConfigTxPowerDbm = -8;
static constexpr uint8_t kConfigMaxPayloadLength = 32U;

RawRadioLink gRadio;
RawRadioPacket gPacket{};
uint32_t gRxCount = 0U;
uint32_t gCrcErrorCount = 0U;
uint32_t gLastHeartbeatMs = 0U;

void ledOn() {
  (void)Gpio::write(kPinUserLed, false);
}

void ledOff() {
  (void)Gpio::write(kPinUserLed, true);
}

void pulse(uint8_t count, uint16_t onMs = 25U, uint16_t offMs = 50U) {
  for (uint8_t i = 0; i < count; ++i) {
    ledOn();
    delay(onMs);
    ledOff();
    if ((i + 1U) < count) {
      delay(offMs);
    }
  }
}

[[noreturn]] void failStage(uint8_t stage) {
  while (true) {
    for (uint8_t i = 0; i < stage; ++i) {
      pulse(1U, 90U, 120U);
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
  config.maxPayloadLength = kConfigMaxPayloadLength;
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
  if (!BoardControl::enableRfPath(BoardAntennaPath::kCeramic)) {
    failStage(1);
  }
}

void printPacket(const RawRadioPacket& packet) {
  if (packet.length < 2U) {
    Serial.println(F("RX short packet"));
    return;
  }

  const uint16_t counter = static_cast<uint16_t>(packet.payload[0]) |
                           (static_cast<uint16_t>(packet.payload[1]) << 8U);

  Serial.print(F("RX counter="));
  Serial.print(counter);
  Serial.print(F(" rssi="));
  Serial.print(packet.rssiDbm);
  Serial.print(F(" payload=\""));
  for (uint8_t i = 2U; i < packet.length; ++i) {
    const char ch = static_cast<char>(packet.payload[i]);
    if ((ch >= 32) && (ch <= 126)) {
      Serial.print(ch);
    } else {
      Serial.print('.');
    }
  }
  Serial.println('"');
}

void pollReceiver() {
  if (!gRadio.receiverArmed()) {
    if (!gRadio.armReceive()) {
      failStage(2);
    }
    return;
  }

  const RawRadioReceiveStatus status = gRadio.pollReceive(&gPacket);
  if (status == RawRadioReceiveStatus::kIdle) {
    return;
  }
  if (status == RawRadioReceiveStatus::kCrcError) {
    ++gCrcErrorCount;
    return;
  }
  if (status == RawRadioReceiveStatus::kError) {
    failStage(2);
  }

  ++gRxCount;
  printPacket(gPacket);
  pulse(2U, 20U, 35U);
}

}  // namespace

void setup() {
  configureBoard();
  if (!gRadio.begin(makeRadioConfig())) {
    failStage(3);
  }

  Serial.println(F("Raw RADIO packet RX"));
  Serial.println(F("Mode: NRF 1Mbit proprietary"));
  Serial.print(F("Listening: BASE0=0x"));
  Serial.print(kConfigAddressBase0, HEX);
  Serial.print(F(" PREFIX0=0x"));
  Serial.print(kConfigAddressPrefix0, HEX);
  Serial.print(F(" FREQ=24"));
  Serial.print(kConfigFrequencyOffsetMhz);
  Serial.print(F(" MHz TX="));
  Serial.print(static_cast<int>(kConfigTxPowerDbm));
  Serial.println(F(" dBm"));
  pulse(1U, 45U, 80U);
}

void loop() {
  pollReceiver();

  const uint32_t now = millis();
  if ((now - gLastHeartbeatMs) >= 1000U) {
    gLastHeartbeatMs = now;
    Serial.print(F("Waiting rx="));
    Serial.print(gRxCount);
    Serial.print(F(" crcerr="));
    Serial.println(gCrcErrorCount);
  }

  delay(5U);
}
