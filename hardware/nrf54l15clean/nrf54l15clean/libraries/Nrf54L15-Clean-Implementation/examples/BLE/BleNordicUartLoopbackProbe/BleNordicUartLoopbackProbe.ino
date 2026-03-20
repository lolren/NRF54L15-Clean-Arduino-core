#include <Arduino.h>

#include "ble_nus.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;
static BleNordicUart g_nus(g_ble);
static PowerManager g_power;

static bool g_wasConnected = false;
static bool g_bannerSent = false;
static uint32_t g_lastStatusMs = 0U;
static uint32_t g_echoedBytes = 0U;
static uint32_t g_droppedBytes = 0U;

static constexpr int8_t kTxPowerDbm = 0;
static constexpr uint8_t kPollBudgetBytes = 16U;
static constexpr uint32_t kStatusPeriodMs = 2000UL;
static constexpr uint8_t kAddress[6] = {0x37, 0x00, 0x15, 0x54, 0xDE, 0xC0};
static constexpr char kGattName[] = "X54 NUS Loopback";
static constexpr char kBanner[] = "X54 NUS loopback ready\r\n";
static const uint8_t kNusAdvPayload[] = {
    2, 0x01, 0x06,
    7, 0x09, 'X', '5', '4', '-', 'L', 'B',
    17, 0x07,
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
};

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

static void queueBanner() {
  if (g_bannerSent || !g_nus.isNotifyEnabled()) {
    return;
  }
  if (g_nus.availableForWrite() < static_cast<int>(sizeof(kBanner) - 1U)) {
    return;
  }
  g_nus.write(reinterpret_cast<const uint8_t*>(kBanner), sizeof(kBanner) - 1U);
  g_bannerSent = true;
}

static void pumpLoopback(uint8_t maxBytes) {
  uint8_t budget = maxBytes;
  while (budget > 0U && g_nus.available() > 0) {
    const int ch = g_nus.read();
    if (ch < 0) {
      break;
    }
    if (g_nus.availableForWrite() <= 0) {
      ++g_droppedBytes;
      break;
    }
    g_nus.write(static_cast<uint8_t>(ch));
    ++g_echoedBytes;
    --budget;
  }
}

void setup() {
  Serial.begin(115200);
  delay(350);
  Serial.print("\r\nBleNordicUartLoopbackProbe start\r\n");

  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  bool ok = BoardControl::setAntennaPath(BoardAntennaPath::kCeramic);
  if (ok) {
    ok = g_ble.begin(kTxPowerDbm) &&
         g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic) &&
         g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd) &&
         g_ble.setAdvertisingData(kNusAdvPayload, sizeof(kNusAdvPayload)) &&
         g_ble.setScanResponseData(nullptr, 0U) &&
         g_ble.setGattDeviceName(kGattName) &&
         g_ble.clearCustomGatt() &&
         g_nus.begin();
  }
  if (ok) {
    g_power.setLatencyMode(PowerLatencyMode::kConstantLatency);
  }

  Serial.print("BLE NUS loopback init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
  if (!ok) {
    while (true) {
      delay(1000);
    }
    return;
  }

  uint8_t addr[6] = {0};
  BleAddressType type = BleAddressType::kPublic;
  if (g_ble.getDeviceAddress(addr, &type)) {
    Serial.print("addr=");
    printAddress(addr);
    Serial.print(" type=");
    Serial.print((type == BleAddressType::kRandomStatic) ? "random" : "public");
    Serial.print("\r\n");
  }
  Serial.print("Advertised as X54-LB\r\n");
}

void loop() {
  if (!g_ble.isConnected()) {
    BleAdvInteraction adv{};
    g_ble.advertiseInteractEvent(&adv, 350U, 350000UL, 700000UL);
    g_nus.service();

    if (g_wasConnected) {
      g_wasConnected = false;
      g_bannerSent = false;
      Gpio::write(kPinUserLed, true);
      Serial.print("BLE client disconnected\r\n");
    }

    if (!g_ble.isConnected()) {
      delay(20);
    }
    return;
  }

  if (!g_wasConnected) {
    g_wasConnected = true;
    g_bannerSent = false;
    Gpio::write(kPinUserLed, false);
    Serial.print("BLE client connected\r\n");
  }

  BleConnectionEvent evt{};
  const bool eventStarted =
      g_ble.pollConnectionEvent(&evt, 300000UL) && evt.eventStarted;
  if (!eventStarted) {
    g_nus.service();
    queueBanner();
    pumpLoopback(2U);
    return;
  }

  g_nus.service(&evt);
  queueBanner();
  pumpLoopback(kPollBudgetBytes);

  const uint32_t nowMs = millis();
  if ((nowMs - g_lastStatusMs) >= kStatusPeriodMs) {
    g_lastStatusMs = nowMs;
    Serial.print("notify=");
    Serial.print(g_nus.isNotifyEnabled() ? "on" : "off");
    Serial.print(" rx_pending=");
    Serial.print(g_nus.available());
    Serial.print(" tx_pending=");
    Serial.print(g_nus.availableForWrite());
    Serial.print(" echoed=");
    Serial.print(g_echoedBytes);
    Serial.print(" dropped=");
    Serial.print(g_droppedBytes);
    Serial.print("\r\n");
  }
}
