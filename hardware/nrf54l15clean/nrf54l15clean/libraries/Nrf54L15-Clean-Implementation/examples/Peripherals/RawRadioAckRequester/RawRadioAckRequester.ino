#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr uint8_t kConfigFrequencyOffsetMhz = 8U;  // 2408 MHz
static constexpr uint32_t kConfigAddressBase0 = 0xC2C2C2C2UL;
static constexpr uint8_t kConfigAddressPrefix0 = 0xC2U;
static constexpr int8_t kConfigTxPowerDbm = -8;
static constexpr uint32_t kConfigExchangeIntervalMs = 1000U;
static constexpr uint32_t kConfigAckWindowUs = 15000U;
static constexpr uint8_t kConfigMaxRetries = 3U;
static constexpr char kConfigPayloadTag[] = "REQ";

static constexpr uint8_t kPacketTypeData = 0xA1U;
static constexpr uint8_t kPacketTypeAck = 0xA2U;
static constexpr uint8_t kDataPayloadLength = 6U;

RawRadioLink gRadio;
RawRadioPacket gRxPacket{};
uint8_t gTxPayload[kDataPayloadLength] = {0};
uint8_t gSequence = 0U;
uint32_t gLastExchangeMs = 0U;

void ledOn() {
  (void)Gpio::write(kPinUserLed, false);
}

void ledOff() {
  (void)Gpio::write(kPinUserLed, true);
}

void pulse(uint8_t count, uint16_t onMs = 20U, uint16_t offMs = 45U) {
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
  config.maxPayloadLength = 16U;
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

void buildDataPayload(uint8_t sequence, uint8_t attempt) {
  gTxPayload[0] = kPacketTypeData;
  gTxPayload[1] = sequence;
  gTxPayload[2] = attempt;
  for (uint8_t i = 0; i < (kDataPayloadLength - 3U); ++i) {
    const char ch = kConfigPayloadTag[i];
    gTxPayload[3U + i] = (ch != '\0') ? static_cast<uint8_t>(ch)
                                       : static_cast<uint8_t>('.');
  }
}

bool exchangeWithAck(uint8_t sequence, uint8_t* outAttemptUsed) {
  if (outAttemptUsed != nullptr) {
    *outAttemptUsed = 0U;
  }

  if (!BoardControl::enableRfPath(BoardAntennaPath::kCeramic)) {
    return false;
  }

  for (uint8_t attempt = 1U; attempt <= kConfigMaxRetries; ++attempt) {
    if (outAttemptUsed != nullptr) {
      *outAttemptUsed = attempt;
    }

    buildDataPayload(sequence, attempt);
    if (!gRadio.transmit(gTxPayload, kDataPayloadLength)) {
      BoardControl::collapseRfPathIdle();
      return false;
    }

    const RawRadioReceiveStatus status =
        gRadio.waitForReceive(&gRxPacket, kConfigAckWindowUs);
    if (status == RawRadioReceiveStatus::kPacket && gRxPacket.length >= 2U &&
        gRxPacket.payload[0] == kPacketTypeAck &&
        gRxPacket.payload[1] == sequence) {
      BoardControl::collapseRfPathIdle();
      return true;
    }
    if (status == RawRadioReceiveStatus::kError) {
      BoardControl::collapseRfPathIdle();
      return false;
    }
  }

  BoardControl::collapseRfPathIdle();
  return false;
}

}  // namespace

void setup() {
  configureBoard();
  if (!gRadio.begin(makeRadioConfig())) {
    failStage(2);
  }

  Serial.println(F("Raw RADIO ACK requester"));
  Serial.println(F("Requester sends REQ and waits for software ACK"));
  Serial.print(F("Pipe: BASE0=0x"));
  Serial.print(kConfigAddressBase0, HEX);
  Serial.print(F(" PREFIX0=0x"));
  Serial.print(kConfigAddressPrefix0, HEX);
  Serial.print(F(" FREQ=24"));
  Serial.print(kConfigFrequencyOffsetMhz);
  Serial.print(F(" MHz TX="));
  Serial.print(static_cast<int>(kConfigTxPowerDbm));
  Serial.println(F(" dBm"));
  pulse(1U, 40U, 70U);
}

void loop() {
  const uint32_t now = millis();
  if ((now - gLastExchangeMs) < kConfigExchangeIntervalMs) {
    delay(10U);
    return;
  }

  gLastExchangeMs = now;
  uint8_t attemptUsed = 0U;
  const bool ok = exchangeWithAck(gSequence, &attemptUsed);

  Serial.print(F("REQ seq="));
  Serial.print(gSequence);
  Serial.print(F(" attempts="));
  Serial.print(attemptUsed);
  Serial.print(F(" result="));
  Serial.println(ok ? F("ACK") : F("TIMEOUT"));

  pulse(ok ? 1U : 2U);
  ++gSequence;
}
