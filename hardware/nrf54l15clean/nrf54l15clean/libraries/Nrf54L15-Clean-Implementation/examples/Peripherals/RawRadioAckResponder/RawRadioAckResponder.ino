#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

namespace {

static constexpr uint8_t kConfigFrequencyOffsetMhz = 8U;  // 2408 MHz
static constexpr uint32_t kConfigAddressBase0 = 0xC2C2C2C2UL;
static constexpr uint8_t kConfigAddressPrefix0 = 0xC2U;
static constexpr int8_t kConfigTxPowerDbm = -8;

static constexpr uint8_t kPacketTypeData = 0xA1U;
static constexpr uint8_t kPacketTypeAck = 0xA2U;
static constexpr uint8_t kDataPayloadLength = 6U;
static constexpr uint8_t kAckPayloadLength = 3U;

RawRadioLink gRadio;
RawRadioPacket gRxPacket{};
uint8_t gAckPayload[kAckPayloadLength] = {0};
uint32_t gAckedCount = 0U;
uint32_t gCrcErrorCount = 0U;
uint32_t gIgnoredCount = 0U;
uint32_t gLastHeartbeatMs = 0U;

void ledOn() {
  (void)Gpio::write(kPinUserLed, false);
}

void ledOff() {
  (void)Gpio::write(kPinUserLed, true);
}

void pulse(uint8_t count, uint16_t onMs = 20U, uint16_t offMs = 35U) {
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
  if (!BoardControl::enableRfPath(BoardAntennaPath::kCeramic)) {
    failStage(1);
  }
}

bool sendAck(uint8_t sequence) {
  gAckPayload[0] = kPacketTypeAck;
  gAckPayload[1] = sequence;
  gAckPayload[2] = 'K';
  return gRadio.transmit(gAckPayload, kAckPayloadLength);
}

void pollReceiver() {
  if (!gRadio.receiverArmed()) {
    if (!gRadio.armReceive()) {
      failStage(2);
    }
    return;
  }

  const RawRadioReceiveStatus status = gRadio.pollReceive(&gRxPacket);
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

  const bool frameOk = (gRxPacket.length >= kDataPayloadLength) &&
                       (gRxPacket.payload[0] == kPacketTypeData);
  if (!frameOk) {
    ++gIgnoredCount;
    return;
  }

  const uint8_t sequence = gRxPacket.payload[1];
  const uint8_t attempt = gRxPacket.payload[2];
  const bool acked = sendAck(sequence);

  Serial.print(F("RX seq="));
  Serial.print(sequence);
  Serial.print(F(" attempt="));
  Serial.print(attempt);
  Serial.print(F(" ack="));
  Serial.println(acked ? F("sent") : F("failed"));

  if (acked) {
    ++gAckedCount;
    pulse(2U);
  } else {
    ++gIgnoredCount;
  }
}

}  // namespace

void setup() {
  configureBoard();
  if (!gRadio.begin(makeRadioConfig())) {
    failStage(3);
  }

  Serial.println(F("Raw RADIO ACK responder"));
  Serial.println(F("Responder listens for REQ and answers with ACK"));
  Serial.print(F("Pipe: BASE0=0x"));
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
    Serial.print(F("ACK sent="));
    Serial.print(gAckedCount);
    Serial.print(F(" ignored="));
    Serial.print(gIgnoredCount);
    Serial.print(F(" crcerr="));
    Serial.println(gCrcErrorCount);
  }

  delay(5U);
}
