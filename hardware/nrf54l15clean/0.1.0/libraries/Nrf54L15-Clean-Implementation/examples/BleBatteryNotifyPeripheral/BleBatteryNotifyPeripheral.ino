#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;
static PowerManager g_power;

static uint8_t g_batteryLevel = 100U;
static uint32_t g_lastBatteryStepMs = 0U;

void setup() {
  Serial.begin(115200);
  delay(350);
  Serial.print("\r\nBleBatteryNotifyPeripheral start\r\n");

  g_power.setLatencyMode(PowerLatencyMode::kLowPower);
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  bool ok = g_ble.begin(-8);
  if (ok) {
    ok = g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd) &&
         g_ble.setAdvertisingName("X54-BATT", true) &&
         g_ble.setScanResponseName("X54-BATT-SCAN") &&
         g_ble.setGattDeviceName("XIAO nRF54L15 Battery") &&
         g_ble.setGattBatteryLevel(g_batteryLevel);
  }

  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
  Serial.print("Enable notifications on Battery Level (0x2A19) CCCD to receive updates.\r\n");
}

void loop() {
  if (!g_ble.isConnected()) {
    BleAdvInteraction adv{};
    g_ble.advertiseInteractEvent(&adv, 350U, 300000UL, 700000UL);
    Gpio::write(kPinUserLed, true);
    __asm volatile("wfi");
    return;
  }

  const uint32_t now = millis();
  if ((now - g_lastBatteryStepMs) >= 5000UL) {
    g_lastBatteryStepMs = now;
    g_batteryLevel = (g_batteryLevel > 5U) ? static_cast<uint8_t>(g_batteryLevel - 1U)
                                            : 100U;
    g_ble.setGattBatteryLevel(g_batteryLevel);

    Serial.print("battery level updated to ");
    Serial.print(g_batteryLevel);
    Serial.print("%\r\n");
  }

  BleConnectionEvent evt{};
  if (!g_ble.pollConnectionEvent(&evt, 450000UL)) {
    __asm volatile("wfi");
    return;
  }

  if (evt.terminateInd) {
    Serial.print("link terminated\r\n");
    Gpio::write(kPinUserLed, true);
    return;
  }

  if (evt.packetReceived && evt.crcOk) {
    Gpio::write(kPinUserLed, false);
  } else {
    Gpio::write(kPinUserLed, true);
  }

  // ATT notification payload on L2CAP ATT channel:
  // [0..1]=len, [2..3]=cid, [4]=opcode, [5..6]=handle, [7]=value.
  if (evt.txPayloadLength >= 8U && evt.txPayload != nullptr &&
      evt.txPayload[4] == 0x1BU && evt.txPayload[5] == 0x12U && evt.txPayload[6] == 0x00U) {
    Serial.print("battery notify sent: ");
    Serial.print(evt.txPayload[7]);
    Serial.print("%\r\n");
  }

  __asm volatile("wfi");
}
