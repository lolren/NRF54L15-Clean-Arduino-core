/*
  Connection Current Zephyr Equivalent - Peripheral

  This sketch mirrors the Zephyr peripheral used in the connection-current
  comparison from GitHub discussion 74:
  - connectable advertising with the same 128-bit service UUID
  - 1M PHY, 23-byte ATT MTU, 50 ms connection interval target
  - 16-byte notify payload once per second
  - 16-byte central reply through Write Without Response

  Build with Tools > Power Profile > WFI / Low Power, VPR disabled, and Thread,
  Matter, and Zigbee disabled when comparing current.
*/

#include <bluefruit.h>
#include <nrf54l15_hal.h>

using xiao_nrf54l15::BoardAntennaPath;
using xiao_nrf54l15::BoardControl;

#define CONN_CURRENT_DEBUG_SERIAL 0
#define CONN_CURRENT_PPK_TEST_PINS 0

// 0 matches the Zephyr sample's always-enabled XIAO RF switch regulator.
// 1 lets the Arduino core power the RF switch only around BLE radio events.
#define CONN_CURRENT_CORE_MANAGED_RF_SWITCH 0

static constexpr int8_t kTxPowerDbm = 8;
static constexpr uint16_t kConnectionIntervalUnits = 40U;  // 40 * 1.25 ms = 50 ms
static constexpr uint32_t kCommunicationIntervalMs = 1000UL;
static constexpr uint8_t kPayloadLength = 16U;

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

BLEService dataService("5500554c-0000-4dd1-be0c-40588193b485");
BLECharacteristic notifyChar("5500554c-0010-4dd1-be0c-40588193b485");
BLECharacteristic writeChar("5500554c-0020-4dd1-be0c-40588193b485");

static void setLedOff() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

static void configureTestPins() {
#if CONN_CURRENT_PPK_TEST_PINS
#if defined(PIN_D0)
  pinMode(PIN_D0, OUTPUT);
  digitalWrite(PIN_D0, LOW);
#endif
#if defined(PIN_D1)
  pinMode(PIN_D1, OUTPUT);
  digitalWrite(PIN_D1, LOW);
#endif
#if defined(PIN_D2)
  pinMode(PIN_D2, OUTPUT);
  digitalWrite(PIN_D2, LOW);
#endif
#if defined(PIN_D3)
  pinMode(PIN_D3, OUTPUT);
  digitalWrite(PIN_D3, LOW);
#endif
#endif
}

static void setTest0(bool level) {
#if CONN_CURRENT_PPK_TEST_PINS && defined(PIN_D0)
  digitalWrite(PIN_D0, level ? HIGH : LOW);
#else
  (void)level;
#endif
}

static void setTest1(bool level) {
#if CONN_CURRENT_PPK_TEST_PINS && defined(PIN_D1)
  digitalWrite(PIN_D1, level ? HIGH : LOW);
#else
  (void)level;
#endif
}

static void setTest2(bool level) {
#if CONN_CURRENT_PPK_TEST_PINS && defined(PIN_D2)
  digitalWrite(PIN_D2, level ? HIGH : LOW);
#else
  (void)level;
#endif
}

static void setTest3(bool level) {
#if CONN_CURRENT_PPK_TEST_PINS && defined(PIN_D3)
  digitalWrite(PIN_D3, level ? HIGH : LOW);
#else
  (void)level;
#endif
}

static void configureBoardPower() {
  BoardControl::setImuMicEnabled(false);
  BoardControl::setBatterySenseEnabled(false);

#if CONN_CURRENT_CORE_MANAGED_RF_SWITCH
  BoardControl::collapseRfPathIdle(BoardAntennaPath::kControlHighImpedance,
                                   true);
#else
  BoardControl::enableRfPath(BoardAntennaPath::kCeramic);
#endif
}

static uint16_t sampleBatteryMilliVolts() {
#if defined(VBAT_EN) && defined(VBAT_READ)
  digitalWrite(VBAT_EN, HIGH);
  delay(5);
  const uint32_t raw = static_cast<uint32_t>(analogRead(VBAT_READ));
  digitalWrite(VBAT_EN, LOW);
  return static_cast<uint16_t>((2UL * 3600UL * raw) / 4096UL);
#else
  int32_t mv = 0;
  if (BoardControl::sampleBatteryMilliVolts(&mv) ||
      BoardControl::sampleVddMilliVolts(&mv)) {
    return (mv > 0) ? static_cast<uint16_t>(mv) : 0U;
  }
  return 0U;
#endif
}

static int8_t currentRssi() {
  const uint16_t connHandle = Bluefruit.connHandle();
  BLEConnection* connection = Bluefruit.Connection(connHandle);
  return (connection != nullptr) ? connection->getRssi() : -127;
}

static void connect_callback(uint16_t connHandle) {
  g_connected = false;

  BLEConnection* connection = Bluefruit.Connection(connHandle);
  (void)connection;

  Bluefruit.Periph.setConnInterval(kConnectionIntervalUnits,
                                   kConnectionIntervalUnits);
  g_written = false;
  g_connected = true;
}

static void disconnect_callback(uint16_t connHandle, uint8_t reason) {
  (void)connHandle;
  (void)reason;
  g_connected = false;
  g_notifyEnabled = false;
  g_written = false;
}

static void cccd_callback(uint16_t connHandle, BLECharacteristic* chr,
                          uint16_t cccdValue) {
  (void)connHandle;
  (void)cccdValue;
  if (chr == &notifyChar) {
    g_notifyEnabled = notifyChar.notifyEnabled();
  }
}

static void written_callback(uint16_t connHandle, BLECharacteristic* chr,
                             uint8_t* data, uint16_t len) {
  (void)connHandle;
  (void)chr;
  setTest3(true);
  const uint16_t copyLen = (len < kPayloadLength) ? len : kPayloadLength;
  memcpy(g_rxBuffer, data, copyLen);
  if (copyLen < kPayloadLength) {
    memset(&g_rxBuffer[copyLen], 0, kPayloadLength - copyLen);
  }
  delay(1);
  g_written = true;
  setTest3(false);
}

void setup() {
#if CONN_CURRENT_DEBUG_SERIAL
  Serial.begin(115200);
#endif

  setLedOff();
  configureTestPins();
  configureBoardPower();
  analogReadResolution(12);

  Bluefruit.configUuid128Count(4);
  Bluefruit.configPrphConn(23, 2, 1, 1);
  Bluefruit.begin();
  Bluefruit.setTxPower(kTxPowerDbm);
  Bluefruit.setConnLedInterval(false);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(kConnectionIntervalUnits,
                                   kConnectionIntervalUnits);

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
  Bluefruit.Advertising.start(0);
}

void loop() {
  if (!g_connected || !g_notifyEnabled) {
    delay(1);
    return;
  }

  setTest0(true);
  const uint32_t timestamp = millis();

  setTest1(true);
  const uint16_t vbat = sampleBatteryMilliVolts();
  setTest1(false);

  g_payload.u32[0] = timestamp;
  g_payload.u32[1] = static_cast<uint32_t>(random(0x7FFFFFFF));
  g_payload.u16[6] = vbat;
  g_payload.u16[7] = static_cast<uint16_t>(currentRssi());

  setTest2(true);
  notifyChar.notify(g_payload.u8, kPayloadLength);
  delay(1);
  setTest2(false);

  const uint32_t waitStart = millis();
  g_written = false;
  while (!g_written &&
         ((millis() - waitStart) <= (kCommunicationIntervalMs + 200UL))) {
    delay(10);
  }

  memcpy(g_payload.u8, g_rxBuffer, sizeof(g_rxBuffer));
  const uint32_t elapsed = millis() - timestamp;
  setTest0(false);

  if (elapsed < kCommunicationIntervalMs) {
    delay(kCommunicationIntervalMs - elapsed);
  }
}
