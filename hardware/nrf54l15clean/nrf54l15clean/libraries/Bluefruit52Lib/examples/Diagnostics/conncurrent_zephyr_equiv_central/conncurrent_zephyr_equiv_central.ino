/*
  Connection Current Zephyr Equivalent - Central

  Pair this with conncurrent_zephyr_equiv_peripheral. It follows the Zephyr
  central's BLE shape closely enough for current comparison:
  - active scan for the same 128-bit service UUID
  - 23-byte ATT MTU / default data length
  - 50 ms connection interval target from the central side
  - subscribe to the notify characteristic
  - reply to each notify using ATT Write Command / Write Without Response

  Serial is disabled by default so opening the monitor is not part of the
  current profile.
*/

#include <bluefruit.h>
#include <nrf54l15_hal.h>

using xiao_nrf54l15::BoardAntennaPath;
using xiao_nrf54l15::BoardControl;

#define CONN_CURRENT_DEBUG_SERIAL 0
#define CONN_CURRENT_CORE_MANAGED_RF_SWITCH 0

static constexpr int8_t kTxPowerDbm = 8;
static constexpr uint8_t kPayloadLength = 16U;

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

BLEClientService dataService("5500554c-0000-4dd1-be0c-40588193b485");
BLEClientCharacteristic notifyChar("5500554c-0010-4dd1-be0c-40588193b485");
BLEClientCharacteristic writeChar("5500554c-0020-4dd1-be0c-40588193b485");

static void setLedOff() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
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

static void scan_callback(ble_gap_evt_adv_report_t* report) {
  if (report == nullptr) {
    return;
  }
  Bluefruit.Central.connect(report);
}

static void connect_callback(uint16_t connHandle) {
  g_connected = false;
  Bluefruit.Scanner.stop();

  if (!dataService.discover(connHandle) ||
      !notifyChar.discover() ||
      !notifyChar.enableNotify() ||
      !writeChar.discover()) {
    Bluefruit.disconnect(connHandle);
    return;
  }

  BLEConnection* connection = Bluefruit.Connection(connHandle);
  if (connection != nullptr) {
    connection->monitorRssi();
  }
  g_notifyReceived = false;
  g_connected = true;
}

static void disconnect_callback(uint16_t connHandle, uint8_t reason) {
  (void)connHandle;
  (void)reason;
  g_connected = false;
  g_notifyReceived = false;
  Bluefruit.Scanner.start(0);
}

static void notify_callback(BLEClientCharacteristic* chr, uint8_t* data,
                            uint16_t len) {
  (void)chr;
  const uint16_t copyLen = (len < kPayloadLength) ? len : kPayloadLength;
  memcpy(g_rxBuffer, data, copyLen);
  if (copyLen < kPayloadLength) {
    memset(&g_rxBuffer[copyLen], 0, kPayloadLength - copyLen);
  }

  writeChar.writeWithoutResponse(g_txBuffer, kPayloadLength);
  g_notifyReceived = true;
}

void setup() {
#if CONN_CURRENT_DEBUG_SERIAL
  Serial.begin(115200);
#endif

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
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.filterUuid(dataService.uuid);
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.setIntervalMS(60, 30);
  Bluefruit.Scanner.start(0);
}

void loop() {
  if (!g_connected || !g_notifyReceived) {
    delay(1);
    return;
  }

  const uint32_t timestamp = millis();
  const uint16_t vbat = sampleBatteryMilliVolts();
  const int8_t rssi = currentRssi();

  memcpy(g_payload.u8, g_rxBuffer, sizeof(g_rxBuffer));
  g_notifyReceived = false;

  g_payload.u32[0] = timestamp;
  g_payload.u32[2] = static_cast<uint32_t>(random(0x7FFFFFFF));
  g_payload.u16[6] = vbat;
  g_payload.u16[7] = static_cast<uint16_t>(rssi);
  memcpy(g_txBuffer, g_payload.u8, kPayloadLength);
}
