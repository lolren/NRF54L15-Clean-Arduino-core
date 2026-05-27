/*
  Connection Current Debug - Arduino Peripheral

  Matches the central debug sketch with Serial1 diagnostics.
  Autonomous peer drop after 10 seconds for repeatable disconnect testing.
*/

#include <bluefruit.h>
#include <nrf54l15_hal.h>

using xiao_nrf54l15::BoardAntennaPath;
using xiao_nrf54l15::BoardControl;

static constexpr int8_t kTxPowerDbm = 8;
static constexpr uint16_t kConnectionIntervalUnits = 40U;
static constexpr uint32_t kCommunicationIntervalMs = 1000UL;
static constexpr uint8_t kPayloadLength = 16U;
static constexpr uint32_t kConnectLedPulseMs = 5UL;
static constexpr uint32_t kStartupSettleMs = 300UL;
static constexpr uint32_t kAutonomousPeerDropMs = 10000UL;

union Payload {
  uint32_t u32[kPayloadLength / 4U];
  uint16_t u16[kPayloadLength / 2U];
  uint8_t u8[kPayloadLength];
};

static Payload g_payload;
static uint8_t g_rxBuffer[kPayloadLength];
static volatile bool g_written = false;
static volatile bool g_connected = false;
static volatile bool g_notifyEnabled = false;
static volatile bool g_pendingLedPulse = false;
static volatile bool g_pendingAdvRestart = false;
static uint32_t g_connectTimestampMs = 0U;
static uint32_t g_lastDiagMs = 0U;
static bool g_autonomousPeerDropDone = false;
static uint32_t g_diagCounter = 0U;

BLEService dataService("5500554c-0000-4dd1-be0c-40588193b485");
BLECharacteristic notifyChar("5500554c-0010-4dd1-be0c-40588193b485");
BLECharacteristic writeChar("5500554c-0020-4dd1-be0c-40588193b485");

static void dbg(const char* msg) {
  Serial1.print(millis());
  Serial1.print(" P ");
  Serial1.println(msg);
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

static void connect_callback(uint16_t connHandle) {
  g_connected = false;
  Bluefruit.Periph.setConnInterval(kConnectionIntervalUnits, kConnectionIntervalUnits);
  g_written = false;
  g_connected = true;
  g_connectTimestampMs = millis();
  g_autonomousPeerDropDone = false;
  dbg("CONN: established");
}

static void disconnect_callback(uint16_t connHandle, uint8_t reason) {
  g_connected = false;
  g_notifyEnabled = false;
  g_written = false;
  g_pendingAdvRestart = true;
  g_autonomousPeerDropDone = false;
  Serial1.print(millis());
  Serial1.print(" P DISC: reason=0x");
  Serial1.println(reason, HEX);
}

static void cccd_callback(uint16_t connHandle, BLECharacteristic* chr, uint16_t cccdValue) {
  if (chr == &notifyChar) {
    g_notifyEnabled = notifyChar.notifyEnabled();
    dbg(g_notifyEnabled ? "NOTIFY: enabled" : "NOTIFY: disabled");
  }
}

static void written_callback(uint16_t connHandle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  const uint16_t copyLen = (len < kPayloadLength) ? len : kPayloadLength;
  memcpy(g_rxBuffer, data, copyLen);
  if (copyLen < kPayloadLength) {
    memset(&g_rxBuffer[copyLen], 0, kPayloadLength - copyLen);
  }
  delay(1);
  g_written = true;
}

void setup() {
  Serial1.begin(115200);
  delay(500);
  dbg("BOOT");

  setLedOff();
  configureBoardPower();
  analogReadResolution(12);

  Bluefruit.configUuid128Count(4);
  Bluefruit.configPrphConn(23, 2, 1, 1);
  Bluefruit.begin();
  Bluefruit.setTxPower(kTxPowerDbm);
  Bluefruit.setConnLedInterval(false);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(kConnectionIntervalUnits, kConnectionIntervalUnits);

  dataService.begin();
  notifyChar.setProperties(CHR_PROPS_NOTIFY);
  notifyChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  notifyChar.setCccdWriteCallback(cccd_callback);
  notifyChar.setFixedLen(kPayloadLength);
  notifyChar.begin();

  writeChar.setProperties(CHR_PROPS_WRITE_WO_RESP);
  writeChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  writeChar.setFixedLen(kPayloadLength);
  writeChar.setWriteCallback(written_callback);
  writeChar.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(dataService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.setIntervalMS(100, 150);
  Bluefruit.Advertising.setFastTimeout(30);

  delay(kStartupSettleMs);
  dbg("ADV: starting");
  Bluefruit.Advertising.start(0);
}

void loop() {
  if (g_pendingAdvRestart && !g_connected) {
    g_pendingAdvRestart = false;
    dbg("ADV: restart");
    Bluefruit.Advertising.start(0);
  }

  pulseConnectLedIfPending();

  // Autonomous peer drop for repeatable disconnect testing
  if (g_connected && !g_autonomousPeerDropDone &&
      (millis() - g_connectTimestampMs) >= kAutonomousPeerDropMs) {
    g_autonomousPeerDropDone = true;
    Serial1.print(millis());
    Serial1.println(" P FORCING_PEER_LOSS");
    __disable_irq();
    while (true) { __NOP(); }
  }

  if (!g_connected || !g_notifyEnabled) {
    return;
  }

  const uint32_t timestamp = millis();
  g_payload.u32[0] = timestamp;
  g_payload.u32[1] = static_cast<uint32_t>(random(0x7FFFFFFF));

  notifyChar.notify(g_payload.u8, kPayloadLength);
  delay(1);

  const uint32_t waitStart = millis();
  g_written = false;
  while (!g_written && ((millis() - waitStart) <= (kCommunicationIntervalMs + 200UL))) {
    delay(10);
  }

  memcpy(g_payload.u8, g_rxBuffer, sizeof(g_rxBuffer));
  if (g_written) g_pendingLedPulse = true;

  const uint32_t elapsed = millis() - timestamp;
  if (elapsed < kCommunicationIntervalMs) {
    delay(kCommunicationIntervalMs - elapsed);
  }

  // Periodic diagnostic dump
  if (millis() - g_lastDiagMs >= 2000UL) {
    g_lastDiagMs = millis();
    g_diagCounter++;
    Serial1.print(millis());
    Serial1.print(" DIAG#");
    Serial1.print(g_diagCounter);
    Serial1.print(" conn=");
    Serial1.print(g_connected);
    Serial1.print(" notify=");
    Serial1.print(g_notifyEnabled);
    Serial1.print(" adv=");
    Serial1.println(Bluefruit.Advertising.isRunning());
  }
}
