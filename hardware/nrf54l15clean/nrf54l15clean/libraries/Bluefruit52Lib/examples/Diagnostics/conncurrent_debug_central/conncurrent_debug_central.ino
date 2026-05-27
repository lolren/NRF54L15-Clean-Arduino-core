/*
  Connection Current Debug - Arduino Central

  Diagnostic version with Serial output on Serial1 (USB).
  Reports: boot, scan state, advertisements found, connect/disconnect events,
  and post-disconnect scan counters.
*/

#include <bluefruit.h>
#include <nrf54l15_hal.h>

using xiao_nrf54l15::BoardAntennaPath;
using xiao_nrf54l15::BoardControl;

static constexpr int8_t kTxPowerDbm = 8;
static constexpr uint8_t kPayloadLength = 16U;
static constexpr uint32_t kConnectLedPulseMs = 5UL;
static constexpr uint32_t kStartupSettleMs = 300UL;

union Payload {
  uint32_t u32[kPayloadLength / 4U];
  uint16_t u16[kPayloadLength / 2U];
  uint8_t u8[kPayloadLength];
};

static Payload g_payload;
static uint8_t g_rxBuffer[kPayloadLength];
static uint8_t g_txBuffer[kPayloadLength];
static volatile bool g_connected = false;
static volatile bool g_notifyReceived = false;
static volatile bool g_pendingLedPulse = false;
static volatile bool g_pendingScanRestart = false;
static bool g_targetPeerValid = false;
static uint8_t g_targetPeerAddress[6] = {0};
static uint8_t g_targetPeerAddrType = BLE_GAP_ADDR_TYPE_PUBLIC;
static uint32_t g_connectTimestampMs = 0U;
static uint32_t g_disconnectTimestampMs = 0U;
static uint32_t g_diagCounter = 0U;
static uint32_t g_scanCallbackCount = 0U;
static uint32_t g_connectCallbackCount = 0U;
static uint32_t g_disconnectCallbackCount = 0U;
static uint32_t g_lastDiagMs = 0U;

BLEClientService dataService("5500554c-0000-4dd1-be0c-40588193b485");
BLEClientCharacteristic notifyChar("5500554c-0010-4dd1-be0c-40588193b485");
BLEClientCharacteristic writeChar("5500554c-0020-4dd1-be0c-40588193b485");

static void dbg(const char* msg) {
  Serial1.print(millis());
  Serial1.print(" ");
  Serial1.println(msg);
}

static void dbgVal(const char* key, uint32_t val) {
  Serial1.print(millis());
  Serial1.print(" ");
  Serial1.print(key);
  Serial1.print("=");
  Serial1.println(val);
}

static void setLedOff() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

static void pulseConnectLedIfPending() {
  if (!g_pendingLedPulse) return;
  g_pendingLedPulse = false;
#if defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, LOW);
  delay(kConnectLedPulseMs);
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

static void configureBoardPower() {
  BoardControl::setImuMicEnabled(false);
  BoardControl::setBatterySenseEnabled(false);
  BoardControl::enableRfPath(BoardAntennaPath::kCeramic);
}

static bool reportMatchesTarget(const ble_gap_evt_adv_report_t* report) {
  if (report == nullptr) return false;
  if (!g_targetPeerValid) return true;
  if (report->peer_addr.addr_type != g_targetPeerAddrType) return false;
  return memcmp(report->peer_addr.addr, g_targetPeerAddress,
                sizeof(g_targetPeerAddress)) == 0;
}

static void scan_callback(ble_gap_evt_adv_report_t* report) {
  g_scanCallbackCount++;
  if (report == nullptr) return;
  if (!reportMatchesTarget(report)) return;
  memcpy(g_targetPeerAddress, report->peer_addr.addr, sizeof(g_targetPeerAddress));
  g_targetPeerAddrType = report->peer_addr.addr_type;
  g_targetPeerValid = true;
  dbg("SCAN: found target, connecting");
  Bluefruit.Central.connect(report);
}

static void connect_callback(uint16_t connHandle) {
  g_connectCallbackCount++;
  g_connected = false;
  Bluefruit.Scanner.stop();
  dbg("CONN: callback entered");
  if (!dataService.discover(connHandle) ||
      !notifyChar.discover() ||
      !notifyChar.enableNotify() ||
      !writeChar.discover()) {
    dbg("CONN: discover/subscribe failed, disconnecting");
    Bluefruit.disconnect(connHandle);
    return;
  }
  g_notifyReceived = false;
  g_connected = true;
  g_connectTimestampMs = millis();
  dbg("CONN: established");
}

static void disconnect_callback(uint16_t connHandle, uint8_t reason) {
  g_disconnectCallbackCount++;
  g_connected = false;
  g_notifyReceived = false;
  g_pendingScanRestart = true;
  g_disconnectTimestampMs = millis();
  Serial1.print(millis());
  Serial1.print(" DISC: reason=0x");
  Serial1.print(reason, HEX);
  Serial1.print(" dt=");
  Serial1.println(g_disconnectTimestampMs - g_connectTimestampMs);
}

static void notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len) {
  (void)chr;
  const uint16_t copyLen = (len < kPayloadLength) ? len : kPayloadLength;
  memcpy(g_rxBuffer, data, copyLen);
  if (copyLen < kPayloadLength) {
    memset(&g_rxBuffer[copyLen], 0, kPayloadLength - copyLen);
  }
  delay(5);
  writeChar.writeWithoutResponse(g_txBuffer, kPayloadLength);
  g_notifyReceived = true;
  g_pendingLedPulse = true;
}

void setup() {
  Serial1.begin(115200);
  delay(500);
  dbg("BOOT");

  setLedOff();
  configureBoardPower();
  analogReadResolution(12);

  Bluefruit.configUuid128Count(4);
  Bluefruit.configCentralConn(23, 2, 1, 1);
  Bluefruit.begin(0, 1);
  Bluefruit.setName("nRF54 Central");
  Bluefruit.setTxPower(kTxPowerDbm);
  Bluefruit.setConnLedInterval(false);

  dataService.begin();
  notifyChar.setNotifyCallback(notify_callback);
  notifyChar.begin();
  writeChar.begin();

  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(false);
  Bluefruit.Scanner.filterUuid(dataService.uuid);
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.setIntervalMS(60, 30);

  delay(kStartupSettleMs);
  dbg("SCAN: starting active scan");
  Bluefruit.Scanner.start(0);
}

void loop() {
  if (g_pendingScanRestart && !g_connected) {
    g_pendingScanRestart = false;
    Bluefruit.Scanner.useActiveScan(false);
    Bluefruit.Scanner.setIntervalMS(200, 100);
    dbg("SCAN: restart passive (200/100)");
    Bluefruit.Scanner.start(0);
  }

  pulseConnectLedIfPending();

  // Periodic diagnostic dump
  if (millis() - g_lastDiagMs >= 2000UL) {
    g_lastDiagMs = millis();
    g_diagCounter++;
    Serial1.print(millis());
    Serial1.print(" DIAG#");
    Serial1.print(g_diagCounter);
    Serial1.print(" conn=");
    Serial1.print(g_connected);
    Serial1.print(" scan_cb=");
    Serial1.print(g_scanCallbackCount);
    Serial1.print(" conn_cb=");
    Serial1.print(g_connectCallbackCount);
    Serial1.print(" disc_cb=");
    Serial1.print(g_disconnectCallbackCount);
    Serial1.print(" target=");
    Serial1.print(g_targetPeerValid);
    Serial1.print(" scanner=");
    Serial1.println(g_scanCallbackCount);
    Serial1.println();
  }

  if (!g_connected) {
    return;
  }

  if (!g_notifyReceived) {
    return;
  }

  const uint32_t timestamp = millis();
  memcpy(g_payload.u8, g_rxBuffer, sizeof(g_rxBuffer));
  g_notifyReceived = false;
  g_payload.u32[0] = timestamp;
  g_payload.u32[2] = static_cast<uint32_t>(random(0x7FFFFFFF));
  memcpy(g_txBuffer, g_payload.u8, kPayloadLength);
}
