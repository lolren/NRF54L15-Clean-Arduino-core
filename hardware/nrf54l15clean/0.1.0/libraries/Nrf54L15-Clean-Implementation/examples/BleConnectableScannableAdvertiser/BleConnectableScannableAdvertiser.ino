#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;
static PowerManager g_power;

static uint32_t g_advEvents = 0;
static uint32_t g_scanReqCount = 0;
static uint32_t g_scanRspCount = 0;
static uint32_t g_connectIndCount = 0;
static uint32_t g_lastLogMs = 0;

static void printAddress(const uint8_t* addr) {
  if (addr == nullptr) {
    return;
  }

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

  Serial.print("\r\nBleConnectableScannableAdvertiser start\r\n");

  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  g_power.setLatencyMode(PowerLatencyMode::kLowPower);

  bool ok = g_ble.begin(0);

  static const uint8_t kAdvPayload[] = {
      2, 0x01, 0x06,                                   // Flags
      8, 0x09, 'X', 'I', 'A', 'O', '-', '5', '4',      // Name
      3, 0x19, 0x80, 0x00                              // Appearance (generic)
  };
  static const uint8_t kScanRspPayload[] = {
      17, 0x09, 'X', 'I', 'A', 'O', '-', '5', '4', '-', 'S', 'C', 'A',
      'N', '-', 'D', 'E', 'M', 'O',
      5, 0xFF, 0x34, 0x12, 0x54, 0x15
  };

  if (ok) {
    ok = g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd);
  }
  if (ok) {
    ok = g_ble.setAdvertisingChannelSelectionAlgorithm2(true);
  }
  if (ok) {
    ok = g_ble.setAdvertisingData(kAdvPayload, sizeof(kAdvPayload));
  }
  if (ok) {
    ok = g_ble.setScanResponseData(kScanRspPayload, sizeof(kScanRspPayload));
  }

  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
}

void loop() {
  BleAdvInteraction interaction{};
  const bool ok = g_ble.advertiseInteractEvent(&interaction, 350U, 350000UL, 700000UL);
  ++g_advEvents;

  if (interaction.receivedScanRequest) {
    ++g_scanReqCount;
    if (interaction.scanResponseTransmitted) {
      ++g_scanRspCount;
    }

    Serial.print("scan_req ch=");
    Serial.print(static_cast<uint8_t>(interaction.channel));
    Serial.print(" peer=");
    printAddress(interaction.peerAddress);
    Serial.print(" rssi=");
    Serial.print(interaction.rssiDbm);
    Serial.print(" rsp=");
    Serial.print(interaction.scanResponseTransmitted ? "TX" : "miss");
    Serial.print("\r\n");
  }

  if (interaction.receivedConnectInd) {
    ++g_connectIndCount;

    Serial.print("connect_ind ch=");
    Serial.print(static_cast<uint8_t>(interaction.channel));
    Serial.print(" initA=");
    printAddress(interaction.peerAddress);
    Serial.print(" rssi=");
    Serial.print(interaction.rssiDbm);
    Serial.print("\r\n");
  }

  // Active-low LED: pulse on interactions.
  const bool interactionSeen = interaction.receivedScanRequest || interaction.receivedConnectInd;
  Gpio::write(kPinUserLed, interactionSeen ? false : true);

  const uint32_t now = millis();
  if ((now - g_lastLogMs) >= 1000UL) {
    g_lastLogMs = now;
    char line[160];
    snprintf(line, sizeof(line),
             "t=%lu adv=%lu scan_req=%lu scan_rsp=%lu conn_ind=%lu status=%s\r\n",
             static_cast<unsigned long>(now),
             static_cast<unsigned long>(g_advEvents),
             static_cast<unsigned long>(g_scanReqCount),
             static_cast<unsigned long>(g_scanRspCount),
             static_cast<unsigned long>(g_connectIndCount),
             ok ? "OK" : "FAIL");
    Serial.print(line);
  }

  // Keep average current low between advertising events.
  __asm volatile("wfi");
}
