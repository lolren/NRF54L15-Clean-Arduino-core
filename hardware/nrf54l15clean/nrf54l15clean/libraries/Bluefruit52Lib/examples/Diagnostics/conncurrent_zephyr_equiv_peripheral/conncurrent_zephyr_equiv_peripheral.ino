/*
  Connection Current Parity - Arduino Peripheral

  Matches the Zephyr current-comparison peripheral as closely as practical:
  - connectable advertising with the same 128-bit service UUID
  - 1M PHY, 23-byte ATT MTU, 50 ms connection interval target
  - 16-byte notify payload once per second
  - 16-byte central reply through Write Without Response

  Green LED pulse matches the Zephyr sketch: 5 ms after each central reply.
*/

#include <bluefruit.h>
#include <nrf54l15_hal.h>

using xiao_nrf54l15::BoardAntennaPath;
using xiao_nrf54l15::BoardControl;

#define CONN_CURRENT_DEBUG_SERIAL 0
#define CONN_CURRENT_CORE_MANAGED_RF_SWITCH 0

static constexpr int8_t kTxPowerDbm = 8;
static constexpr uint16_t kConnectionIntervalUnits = 40U;
static constexpr uint32_t kCommunicationIntervalMs = 1000UL;
static constexpr uint8_t kPayloadLength = 16U;
static constexpr uint32_t kConnectLedPulseMs = 5UL;
static constexpr uint32_t kStartupSettleMs = 300UL;
static constexpr char kPeripheralName[] = "nRF54 Peripheral";

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
static volatile uint32_t g_notifyCount = 0U;
static volatile uint32_t g_writeCount = 0U;
static volatile uint8_t g_addrType = 0U;
static volatile uint8_t g_addrBytes[6] = {0};
static volatile uint8_t g_setAddrOk = 0U;
static volatile uint8_t g_advStarted = 0U;

BLEService dataService("5500554c-0000-4dd1-be0c-40588193b485");
BLECharacteristic notifyChar("5500554c-0010-4dd1-be0c-40588193b485");
BLECharacteristic writeChar("5500554c-0020-4dd1-be0c-40588193b485");

static void setLedOff() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

static void pulseConnectLedIfPending() {
  if (!g_pendingLedPulse) {
    return;
  }

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
  (void)connHandle;

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
  g_pendingAdvRestart = true;
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
  const uint16_t copyLen = (len < kPayloadLength) ? len : kPayloadLength;
  memcpy(g_rxBuffer, data, copyLen);
  if (copyLen < kPayloadLength) {
    memset(&g_rxBuffer[copyLen], 0, kPayloadLength - copyLen);
  }

  delay(1);
  ++g_writeCount;
  g_written = true;
}

void setup() {
#if CONN_CURRENT_DEBUG_SERIAL
  Serial.begin(115200);
#endif

  setLedOff();
  configureBoardPower();
  analogReadResolution(12);

  Bluefruit.configUuid128Count(4);
  Bluefruit.configPrphConn(23, 2, 1, 1);
  Bluefruit.begin();
  g_setAddrOk = 0U;
  g_addrType = Bluefruit.getAddr(const_cast<uint8_t*>(g_addrBytes));
  Bluefruit.setName("XIAO_nRF54L15");
  Bluefruit.setTxPower(kTxPowerDbm);
  Bluefruit.setConnLedInterval(false);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(kConnectionIntervalUnits,
                                   kConnectionIntervalUnits);
  Bluefruit.Periph.setConnSupervisionTimeoutMS(200);  // 200ms supervision timeout

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
  g_advStarted = Bluefruit.Advertising.start(0) ? 1U : 0U;
}

void loop() {
  if (g_pendingAdvRestart && !g_connected) {
    g_pendingAdvRestart = false;
    Bluefruit.Advertising.start(0);
  }

  pulseConnectLedIfPending();

  if (!g_connected || !g_notifyEnabled) {
    delay(1);
    return;
  }

  const uint32_t timestamp = millis();
  const uint16_t vbat = sampleBatteryMilliVolts();

  g_payload.u32[0] = timestamp;
  g_payload.u32[1] = static_cast<uint32_t>(random(0x7FFFFFFF));
  g_payload.u16[6] = vbat;
  g_payload.u16[7] = static_cast<uint16_t>(currentRssi());

  if (notifyChar.notify(g_payload.u8, kPayloadLength)) {
    ++g_notifyCount;
  }
  delay(1);

  const uint32_t waitStart = millis();
  g_written = false;
  while (!g_written &&
         ((millis() - waitStart) <= (kCommunicationIntervalMs + 200UL))) {
    delay(10);
  }

  memcpy(g_payload.u8, g_rxBuffer, sizeof(g_rxBuffer));
  if (g_written) {
    g_pendingLedPulse = true;
  }
  const uint32_t elapsed = millis() - timestamp;

  if (elapsed < kCommunicationIntervalMs) {
    delay(kCommunicationIntervalMs - elapsed);
  }
}
