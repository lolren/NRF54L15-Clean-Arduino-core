#include <Arduino.h>

#include "ble_nus.h"

using namespace xiao_nrf54l15;

namespace {

BleRadio g_ble;
BleNordicUart g_nus(g_ble);
PowerManager g_power;

constexpr uint16_t kUsbToBleBufferSize = 1024U;
constexpr uint8_t kUsbToBleChunkBytes = 20U;
constexpr uint16_t kUsbStageBudgetBytes = 128U;
constexpr uint32_t kConnectionPollTimeoutUs = 3000UL;
constexpr uint32_t kStatusPeriodMs = 2000UL;
constexpr uint8_t kNoBytes = 0U;

bool g_wasConnected = false;
bool g_bannerSent = false;
uint32_t g_lastStatusMs = 0U;
uint32_t g_connSessionRxDropped = 0U;
uint32_t g_connSessionTxDropped = 0U;
uint16_t g_usbToBleHead = 0U;
uint16_t g_usbToBleTail = 0U;
uint16_t g_usbToBleCount = 0U;
uint8_t g_usbToBleBuffer[kUsbToBleBufferSize] = {0};

}  // namespace

constexpr int8_t kTxPowerDbm = 0;
constexpr uint8_t kAddress[6] = {0x35, 0x00, 0x15, 0x54, 0xDE, 0xC0};
constexpr uint8_t kNusScanResponse[] = {
    17, 0x07,
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E};

static const char* disconnectReasonLabel(uint8_t reason) {
  switch (reason) {
    case 0x08:
      return "timeout";
    case 0x13:
      return "remote user";
    case 0x16:
      return "local host";
    case 0x3D:
      return "mic failure";
    default:
      return "other";
  }
}

static uint16_t advanceIndex(uint16_t index, uint16_t capacity) {
  ++index;
  if (index >= capacity) {
    index = 0U;
  }
  return index;
}

static void resetBridgeBuffers() {
  g_usbToBleHead = 0U;
  g_usbToBleTail = 0U;
  g_usbToBleCount = 0U;
}

static void stageUsbToBle(int maxBytes) {
  const int queueSpace = static_cast<int>(kUsbToBleBufferSize - g_usbToBleCount);
  if (queueSpace <= 0) {
    return;
  }
  int budget = maxBytes;
  if (budget > queueSpace) {
    budget = queueSpace;
  }
  while (budget > 0 && Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch < 0) {
      break;
    }

    g_usbToBleBuffer[g_usbToBleHead] = static_cast<uint8_t>(ch);
    g_usbToBleHead = advanceIndex(g_usbToBleHead, kUsbToBleBufferSize);
    ++g_usbToBleCount;
    --budget;
  }
}

static void pumpUsbToBle() {
  if (!g_nus.isNotifyEnabled() || g_usbToBleCount == 0U) {
    return;
  }

  const int payloadLimit = static_cast<int>(g_nus.maxPayloadLength());
  if (payloadLimit <= 0) {
    return;
  }
  int budget = g_nus.availableForWrite();
  if (budget > static_cast<int>(payloadLimit)) {
    budget = static_cast<int>(payloadLimit);
  }
  if (budget > g_usbToBleCount) {
    budget = static_cast<int>(g_usbToBleCount);
  }
  if (budget > static_cast<int>(kUsbToBleChunkBytes)) {
    budget = static_cast<int>(kUsbToBleChunkBytes);
  }

  if (budget <= 0) {
    return;
  }

  uint8_t chunk[kUsbToBleChunkBytes] = {kNoBytes};
  uint16_t index = g_usbToBleTail;
  for (int i = 0; i < budget; ++i) {
    chunk[i] = g_usbToBleBuffer[index];
    index = advanceIndex(index, kUsbToBleBufferSize);
  }
  const size_t written = g_nus.write(chunk, static_cast<size_t>(budget));
  for (size_t i = 0; i < written; ++i) {
    g_usbToBleTail = advanceIndex(g_usbToBleTail, kUsbToBleBufferSize);
    --g_usbToBleCount;
  }
}

static void printDropCounters() {
  const uint32_t txDropped = g_nus.txDroppedBytes();
  const uint32_t rxDropped = g_nus.rxDroppedBytes();
  if (txDropped <= g_connSessionTxDropped && rxDropped <= g_connSessionRxDropped) {
    return;
  }

  Serial.print("bridge_drops tx=");
  Serial.print(txDropped - g_connSessionTxDropped);
  Serial.print(" rx=");
  Serial.print(rxDropped - g_connSessionRxDropped);
  Serial.print(" queued=");
  Serial.print(g_usbToBleCount);
  Serial.print("\r\n");
}

static void printStreamingStats() {
  const uint32_t nowMs = millis();
  if ((nowMs - g_lastStatusMs) < kStatusPeriodMs) {
    return;
  }
  g_lastStatusMs = nowMs;
  Serial.print("stats pending_tx=");
  Serial.print(g_nus.availableForWrite());
  Serial.print(" usb_q=");
  Serial.print(g_usbToBleCount);
  Serial.print(" usb_avail=");
  Serial.print(Serial.available());
  Serial.print(" rxq=");
  Serial.print(g_nus.available());
  Serial.print(" mtu=");
  Serial.print(g_nus.maxPayloadLength());
  Serial.print(" notify=");
  Serial.print(g_nus.isNotifyEnabled() ? "on" : "off");
  Serial.print("\r\n");
}

static void pumpUsbToBleWhenReady(int maxBytes) {
  stageUsbToBle(maxBytes);
  if (g_nus.isNotifyEnabled() && g_usbToBleCount > 0U) {
    pumpUsbToBle();
  }
}

static void pumpBleToUsb(int maxBytes) {
  int budget = maxBytes;
  while (budget > 0 && g_nus.available() > 0) {
    const int ch = g_nus.read();
    if (ch < 0) {
      break;
    }
    Serial.write(static_cast<uint8_t>(ch));
    --budget;
  }
}

void setup() {
  Serial.begin(115200);
  delay(350);
  Serial.print("\r\nBleNordicUartBridge start\r\n");

  g_power.setLatencyMode(PowerLatencyMode::kConstantLatency);
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  bool ok = BoardControl::setAntennaPath(BoardAntennaPath::kCeramic);
  if (ok) {
    ok = g_ble.begin(kTxPowerDbm) &&
         g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic) &&
         g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd) &&
         g_ble.setAdvertisingName("X54-NUS", true) &&
         g_ble.setScanResponseData(kNusScanResponse, sizeof(kNusScanResponse)) &&
         g_ble.setGattDeviceName("X54 NUS Bridge") &&
         g_ble.clearCustomGatt() && g_nus.begin();
    g_ble.setBackgroundConnectionServiceEnabled(false);
  }

  Serial.print("BLE NUS init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
  if (ok) {
    Serial.print("Advertised as X54-NUS. Open a Nordic UART client and bridge bytes.\r\n");
  }
}

void loop() {
  if (!g_ble.isConnected()) {
    BleAdvInteraction adv{};
    g_ble.advertiseInteractEvent(&adv, 350U, 350000UL, 700000UL);
    g_nus.service();

    if (g_wasConnected) {
      g_wasConnected = false;
      g_bannerSent = false;
      resetBridgeBuffers();
      printDropCounters();
      printStreamingStats();
      Gpio::write(kPinUserLed, true);
      Serial.print("\r\nBLE client disconnected\r\n");
    }

    delay(20);
    return;
  }

  if (!g_wasConnected) {
    g_wasConnected = true;
    g_bannerSent = false;
    g_connSessionRxDropped = g_nus.rxDroppedBytes();
    g_connSessionTxDropped = g_nus.txDroppedBytes();
    resetBridgeBuffers();
    Gpio::write(kPinUserLed, false);
    Serial.print("\r\nBLE client connected\r\n");
  }

  // Keep BLE connection-event polling tight; missing anchors leads to 0x08 timeouts.
  BleConnectionEvent evt{};
  const bool eventStarted =
      g_ble.pollConnectionEvent(&evt, kConnectionPollTimeoutUs) && evt.eventStarted;
  if (!eventStarted) {
    g_nus.service();
    stageUsbToBle(2);
    pumpUsbToBleWhenReady(2);
    return;
  }

  g_nus.service(&evt);
  if (evt.terminateInd) {
    Serial.print("BLE link terminated");
    if (evt.disconnectReasonValid) {
      Serial.print(" reason=0x");
      if (evt.disconnectReason < 0x10U) {
        Serial.print('0');
      }
      Serial.print(evt.disconnectReason, HEX);
      Serial.print(" (");
      Serial.print(disconnectReasonLabel(evt.disconnectReason));
      Serial.print(", ");
      Serial.print(evt.disconnectReasonRemote ? "peer" : "local");
      Serial.print(")");
    }
    Serial.print("\r\n");
    printDropCounters();
  }

  // Only pump UART after a real connection event.
  pumpUsbToBleWhenReady(static_cast<int>(kUsbStageBudgetBytes));
  pumpBleToUsb(16);
  printStreamingStats();

  if (!g_bannerSent && g_nus.isNotifyEnabled()) {
    g_bannerSent = true;
    g_nus.print("X54 Nordic UART bridge ready\r\n");
  }
}
