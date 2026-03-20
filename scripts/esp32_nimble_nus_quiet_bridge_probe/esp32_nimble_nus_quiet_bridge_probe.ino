#include <Arduino.h>
#include <NimBLEDevice.h>

namespace {

constexpr char kDeviceName[] = "ESP32-QBR";
constexpr size_t kUsbToBleBufferSize = 1024U;
constexpr size_t kBleToUsbBufferSize = 1024U;
constexpr size_t kBleChunkBytes = 20U;
constexpr char kSummaryBegin[] = "@@SUMMARY_BEGIN@@";
constexpr char kSummaryEnd[] = "@@SUMMARY_END@@";
constexpr uint32_t kFnv1aOffset = 2166136261UL;
constexpr uint32_t kFnv1aPrime = 16777619UL;

const NimBLEUUID kServiceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
const NimBLEUUID kRxUuid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
const NimBLEUUID kTxUuid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

struct DirectionStats {
  uint32_t bytesIn;
  uint32_t bytesOut;
  uint32_t hashIn;
  uint32_t hashOut;
};

struct SessionStats {
  DirectionStats usbToBle;
  DirectionStats bleToUsb;
  uint32_t usbToBleStageDropped;
  uint32_t bleToUsbStageDropped;
  uint32_t bleRxDropped;
  uint32_t bleTxDropped;
  uint32_t sessionIndex;
  uint32_t connectedAtMs;
  uint16_t connHandle;
  int disconnectReason;
  bool disconnectReasonValid;
};

NimBLEServer* g_server = nullptr;
NimBLECharacteristic* g_rxCharacteristic = nullptr;
NimBLECharacteristic* g_txCharacteristic = nullptr;
bool g_connected = false;
bool g_notifyEnabled = false;
bool g_summaryPending = false;
SessionStats g_stats{};

size_t g_usbToBleHead = 0U;
size_t g_usbToBleTail = 0U;
size_t g_usbToBleCount = 0U;
size_t g_bleToUsbHead = 0U;
size_t g_bleToUsbTail = 0U;
size_t g_bleToUsbCount = 0U;
uint8_t g_usbToBleBuffer[kUsbToBleBufferSize] = {0};
uint8_t g_bleToUsbBuffer[kBleToUsbBufferSize] = {0};

uint32_t fnv1aAppend(uint32_t hash, uint8_t value) {
  return (hash ^ value) * kFnv1aPrime;
}

void resetDirection(DirectionStats* stats) {
  if (stats == nullptr) {
    return;
  }
  stats->bytesIn = 0U;
  stats->bytesOut = 0U;
  stats->hashIn = kFnv1aOffset;
  stats->hashOut = kFnv1aOffset;
}

void resetBuffers() {
  g_usbToBleHead = 0U;
  g_usbToBleTail = 0U;
  g_usbToBleCount = 0U;
  g_bleToUsbHead = 0U;
  g_bleToUsbTail = 0U;
  g_bleToUsbCount = 0U;
}

void beginSession(uint16_t connHandle) {
  resetBuffers();
  resetDirection(&g_stats.usbToBle);
  resetDirection(&g_stats.bleToUsb);
  g_stats.usbToBleStageDropped = 0U;
  g_stats.bleToUsbStageDropped = 0U;
  g_stats.bleRxDropped = 0U;
  g_stats.bleTxDropped = 0U;
  g_stats.connectedAtMs = millis();
  g_stats.connHandle = connHandle;
  g_stats.disconnectReason = 0;
  g_stats.disconnectReasonValid = false;
  ++g_stats.sessionIndex;
  g_summaryPending = false;
}

size_t advanceIndex(size_t index, size_t capacity) {
  ++index;
  if (index >= capacity) {
    index = 0U;
  }
  return index;
}

void printHex32(uint32_t value) {
  Serial.print("0x");
  for (int shift = 28; shift >= 0; shift -= 4) {
    Serial.print(static_cast<uint8_t>((value >> shift) & 0x0FU), HEX);
  }
}

void printSummary() {
  Serial.print("\r\n");
  Serial.print(kSummaryBegin);
  Serial.print("\r\n");
  Serial.print("session=");
  Serial.print(g_stats.sessionIndex);
  Serial.print("\r\n");
  Serial.print("connected_ms=");
  Serial.print(millis() - g_stats.connectedAtMs);
  Serial.print("\r\n");
  Serial.print("disconnect_reason=");
  if (g_stats.disconnectReasonValid) {
    printHex32(static_cast<uint32_t>(g_stats.disconnectReason));
  } else {
    Serial.print("none");
  }
  Serial.print("\r\n");
  Serial.print("disconnect_remote=1\r\n");

  Serial.print("usb_to_ble_in=");
  Serial.print(g_stats.usbToBle.bytesIn);
  Serial.print("\r\n");
  Serial.print("usb_to_ble_out=");
  Serial.print(g_stats.usbToBle.bytesOut);
  Serial.print("\r\n");
  Serial.print("usb_to_ble_in_hash=");
  printHex32(g_stats.usbToBle.hashIn);
  Serial.print("\r\n");
  Serial.print("usb_to_ble_out_hash=");
  printHex32(g_stats.usbToBle.hashOut);
  Serial.print("\r\n");

  Serial.print("ble_to_usb_in=");
  Serial.print(g_stats.bleToUsb.bytesIn);
  Serial.print("\r\n");
  Serial.print("ble_to_usb_out=");
  Serial.print(g_stats.bleToUsb.bytesOut);
  Serial.print("\r\n");
  Serial.print("ble_to_usb_in_hash=");
  printHex32(g_stats.bleToUsb.hashIn);
  Serial.print("\r\n");
  Serial.print("ble_to_usb_out_hash=");
  printHex32(g_stats.bleToUsb.hashOut);
  Serial.print("\r\n");

  Serial.print("usb_to_ble_stage_drop=");
  Serial.print(g_stats.usbToBleStageDropped);
  Serial.print("\r\n");
  Serial.print("ble_to_usb_stage_drop=");
  Serial.print(g_stats.bleToUsbStageDropped);
  Serial.print("\r\n");
  Serial.print("ble_rx_drop=");
  Serial.print(g_stats.bleRxDropped);
  Serial.print("\r\n");
  Serial.print("ble_tx_drop=");
  Serial.print(g_stats.bleTxDropped);
  Serial.print("\r\n");
  Serial.print("usb_to_ble_pending=");
  Serial.print(g_usbToBleCount);
  Serial.print("\r\n");
  Serial.print("ble_to_usb_pending=");
  Serial.print(g_bleToUsbCount);
  Serial.print("\r\n");
  Serial.print(kSummaryEnd);
  Serial.print("\r\n");
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    (void)server;
    g_connected = true;
    g_notifyEnabled = false;
    beginSession(connInfo.getConnHandle());
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    (void)connInfo;
    g_connected = false;
    g_notifyEnabled = false;
    g_stats.disconnectReason = reason;
    g_stats.disconnectReasonValid = true;
    g_summaryPending = true;
    NimBLEDevice::startAdvertising();
  }
} g_serverCallbacks;

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    const std::string value = characteristic->getValue();
    for (size_t i = 0; i < value.size(); ++i) {
      const uint8_t byte = static_cast<uint8_t>(value[i]);
      ++g_stats.bleToUsb.bytesIn;
      g_stats.bleToUsb.hashIn = fnv1aAppend(g_stats.bleToUsb.hashIn, byte);
      if (g_bleToUsbCount >= kBleToUsbBufferSize) {
        ++g_stats.bleToUsbStageDropped;
        continue;
      }
      g_bleToUsbBuffer[g_bleToUsbHead] = byte;
      g_bleToUsbHead = advanceIndex(g_bleToUsbHead, kBleToUsbBufferSize);
      ++g_bleToUsbCount;
    }
  }

  void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
    (void)characteristic;
    g_notifyEnabled = (subValue & 0x01U) != 0U;
    g_stats.connHandle = connInfo.getConnHandle();
  }
} g_rxCallbacks;

void stageUsbToBle(int maxBytes) {
  int budget = maxBytes;
  while (budget > 0 && Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch < 0) {
      break;
    }
    const uint8_t byte = static_cast<uint8_t>(ch);
    ++g_stats.usbToBle.bytesIn;
    g_stats.usbToBle.hashIn = fnv1aAppend(g_stats.usbToBle.hashIn, byte);
    if (g_usbToBleCount >= kUsbToBleBufferSize) {
      ++g_stats.usbToBleStageDropped;
    } else {
      g_usbToBleBuffer[g_usbToBleHead] = byte;
      g_usbToBleHead = advanceIndex(g_usbToBleHead, kUsbToBleBufferSize);
      ++g_usbToBleCount;
    }
    --budget;
  }
}

void pumpUsbToBle() {
  if (!g_connected || !g_notifyEnabled || g_usbToBleCount == 0U || g_txCharacteristic == nullptr) {
    return;
  }

  size_t chunkLength = g_usbToBleCount;
  if (chunkLength > kBleChunkBytes) {
    chunkLength = kBleChunkBytes;
  }
  uint8_t chunk[kBleChunkBytes] = {0};
  size_t index = g_usbToBleTail;
  for (size_t i = 0; i < chunkLength; ++i) {
    chunk[i] = g_usbToBleBuffer[index];
    index = advanceIndex(index, kUsbToBleBufferSize);
  }

  if (!g_txCharacteristic->notify(chunk, chunkLength, g_stats.connHandle)) {
    ++g_stats.bleTxDropped;
    return;
  }

  for (size_t i = 0; i < chunkLength; ++i) {
    const uint8_t byte = g_usbToBleBuffer[g_usbToBleTail];
    ++g_stats.usbToBle.bytesOut;
    g_stats.usbToBle.hashOut = fnv1aAppend(g_stats.usbToBle.hashOut, byte);
    g_usbToBleTail = advanceIndex(g_usbToBleTail, kUsbToBleBufferSize);
    --g_usbToBleCount;
  }
}

void pumpBleToUsb(int maxBytes) {
  int budget = maxBytes;
  while (budget > 0 && g_bleToUsbCount > 0U) {
    const uint8_t byte = g_bleToUsbBuffer[g_bleToUsbTail];
    if (Serial.write(byte) != 1U) {
      break;
    }
    ++g_stats.bleToUsb.bytesOut;
    g_stats.bleToUsb.hashOut = fnv1aAppend(g_stats.bleToUsb.hashOut, byte);
    g_bleToUsbTail = advanceIndex(g_bleToUsbTail, kBleToUsbBufferSize);
    --g_bleToUsbCount;
    --budget;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.print("\r\nESP32 NimBLE Quiet Bridge start\r\n");

  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);

  g_server = NimBLEDevice::createServer();
  g_server->setCallbacks(&g_serverCallbacks);

  NimBLEService* service = g_server->createService(kServiceUuid);
  g_rxCharacteristic =
      service->createCharacteristic(kRxUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  g_txCharacteristic =
      service->createCharacteristic(kTxUuid, NIMBLE_PROPERTY::NOTIFY);

  g_rxCharacteristic->setCallbacks(&g_rxCallbacks);
  g_txCharacteristic->setCallbacks(&g_rxCallbacks);

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->setName(kDeviceName);
  advertising->addServiceUUID(kServiceUuid);
  advertising->enableScanResponse(false);
  advertising->start();

  Serial.print("BLE NUS quiet bridge init: OK\r\n");
  Serial.print("addr=");
  Serial.print(NimBLEDevice::getAddress().toString().c_str());
  Serial.print("\r\n");
  Serial.print("Advertised as ");
  Serial.print(kDeviceName);
  Serial.print("\r\n");
}

void loop() {
  if (g_connected) {
    stageUsbToBle(128);
    pumpUsbToBle();
    pumpBleToUsb(128);
  } else if (g_summaryPending) {
    printSummary();
    resetBuffers();
    g_summaryPending = false;
  }

  delay(1);
}
