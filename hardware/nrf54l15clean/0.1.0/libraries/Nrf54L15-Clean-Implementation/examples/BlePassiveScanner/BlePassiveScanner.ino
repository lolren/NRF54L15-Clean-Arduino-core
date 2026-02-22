#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;
static uint32_t g_seenPackets = 0;

static const char* pduTypeName(uint8_t type) {
  switch (type & 0x0FU) {
    case 0x00:
      return "ADV_IND";
    case 0x01:
      return "ADV_DIRECT";
    case 0x02:
      return "ADV_NONCONN";
    case 0x03:
      return "SCAN_REQ";
    case 0x04:
      return "SCAN_RSP";
    case 0x05:
      return "CONNECT_IND";
    case 0x06:
      return "ADV_SCAN";
    default:
      return "OTHER";
  }
}

static void printAddress(const uint8_t* addr) {
  if (addr == nullptr) {
    return;
  }

  // BLE addresses are typically shown MSB first.
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

  Serial.print("\r\nBlePassiveScanner start\r\n");
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  const bool ok = g_ble.begin(-8);
  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
}

void loop() {
  BleScanPacket pkt{};
  const bool got = g_ble.scanCycle(&pkt, 250000UL);

  if (!got) {
    __asm volatile("wfi");
    return;
  }

  ++g_seenPackets;
  Gpio::write(kPinUserLed, (g_seenPackets & 0x1U) == 0U);

  const uint8_t pduType = pkt.pduHeader & 0x0FU;
  const bool txRandom = ((pkt.pduHeader >> 6U) & 0x1U) != 0U;

  Serial.print("#");
  Serial.print(g_seenPackets);
  Serial.print(" ch=");
  Serial.print(static_cast<uint8_t>(pkt.channel));
  Serial.print(" rssi=");
  Serial.print(pkt.rssiDbm);
  Serial.print(" type=");
  Serial.print(pduTypeName(pduType));
  Serial.print(" txAddr=");
  Serial.print(txRandom ? "random" : "public");
  Serial.print(" len=");
  Serial.print(pkt.length);

  if (pkt.length >= 6U && pkt.payload != nullptr) {
    Serial.print(" advA=");
    printAddress(pkt.payload);
  }

  Serial.print("\r\n");
}
