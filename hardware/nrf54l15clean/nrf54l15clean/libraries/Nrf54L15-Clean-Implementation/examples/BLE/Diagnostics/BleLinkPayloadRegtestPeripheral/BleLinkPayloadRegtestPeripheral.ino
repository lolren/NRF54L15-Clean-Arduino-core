/*
  BLE link payload regression peripheral

  Flash this sketch to one nRF54L15 board and
  BleLinkPayloadRegtestCentral to the other board. By default the link is
  plain/insecure. For LE Secure Connections testing, compile both sketches with:

    -DBLE_LINK_REGTEST_SECURE=1

  The peripheral advertises as X54-LINKTEST, exposes a notify characteristic
  and a write characteristic, then echoes every received write back as a
  notification. This verifies ATT write, notification, DLE, MTU and L2CAP
  reassembly in both directions.

  Serial commands at 115200 baud:
    status  - print link state
    clear   - clear stored bond data
*/

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

#if !defined(BLE_LINK_REGTEST_SECURE)
#define BLE_LINK_REGTEST_SECURE 0
#endif
#if !defined(BLE_LINK_REGTEST_VERBOSE)
#define BLE_LINK_REGTEST_VERBOSE 0
#endif

namespace {

BleRadio g_ble;
Preferences g_prefs;

constexpr char kAdvName[] = "X54-LINKTEST";
constexpr int8_t kTxPowerDbm = 0;
constexpr uint32_t kPollUs = 450000UL;
constexpr uint32_t kSecurityRequestRetryMs = 2000UL;
constexpr uint16_t kServiceUuid = 0x5C00U;
constexpr uint16_t kNotifyUuid = 0x5C01U;
constexpr uint16_t kWriteUuid = 0x5C02U;
constexpr char kPrefsNs[] = "ble_link_reg";
constexpr char kBondKey[] = "bond";
constexpr bool kSecure = (BLE_LINK_REGTEST_SECURE != 0);
constexpr bool kVerbose = (BLE_LINK_REGTEST_VERBOSE != 0);

uint16_t g_serviceHandle = 0U;
uint16_t g_notifyHandle = 0U;
uint16_t g_notifyCccdHandle = 0U;
uint16_t g_writeHandle = 0U;
uint8_t g_echoBuffer[BleRadio::kCustomGattMaxValueLength];
uint8_t g_echoLength = 0U;
bool g_echoPending = false;
bool g_prevConnected = false;
bool g_prevEncrypted = false;
uint32_t g_lastSecurityRequestMs = 0U;
uint32_t g_writeCount = 0U;
uint32_t g_notifyQueueCount = 0U;

bool loadBond(BleBondRecord* outRecord, void*) {
  if (!kSecure ||
      outRecord == nullptr ||
      g_prefs.getBytesLength(kBondKey) != sizeof(BleBondRecord)) {
    return false;
  }
  g_prefs.getBytes(kBondKey, outRecord, sizeof(BleBondRecord));
  Serial.println("PERIPH bond-loaded");
  return true;
}

bool saveBond(const BleBondRecord* record, void*) {
  if (!kSecure || record == nullptr) {
    return false;
  }
  g_prefs.putBytes(kBondKey, record, sizeof(BleBondRecord));
  Serial.println("PERIPH bond-saved");
  return true;
}

bool clearBond(void*) {
  g_prefs.remove(kBondKey);
  return true;
}

void printStatus() {
  int8_t rssiDbm = 0;
  (void)g_ble.getLatestConnectionRssiDbm(&rssiDbm);
  Serial.print("PERIPH status secure=");
  Serial.print(kSecure ? 1 : 0);
  Serial.print(" conn=");
  Serial.print(g_ble.isConnected() ? 1 : 0);
  Serial.print(" enc=");
  Serial.print(g_ble.isConnectionEncrypted() ? 1 : 0);
  Serial.print(" bond=");
  Serial.print(g_ble.hasBondRecord() ? 1 : 0);
  Serial.print(" mtu=");
  Serial.print(g_ble.currentAttMtu());
  Serial.print(" dle=");
  Serial.print(g_ble.currentDataLength());
  Serial.print(" phy=");
  Serial.print(g_ble.currentTxPhy());
  Serial.print(" rssi=");
  Serial.print(rssiDbm);
  Serial.print(" writes=");
  Serial.print(g_writeCount);
  Serial.print(" notify_q=");
  Serial.println(g_notifyQueueCount);
}

void printFailureDebug() {
  BleSecureConnectionsDebugState sc{};
  g_ble.getSecureConnectionsDebugState(&sc);
  Serial.print("PERIPH sc active=");
  Serial.print(sc.active);
  Serial.print(" init=");
  Serial.print(sc.localInitiator);
  Serial.print(" pk=");
  Serial.print(sc.publicKeySent);
  Serial.print(" conf=");
  Serial.print(sc.confirmSent);
  Serial.print(" rand=");
  Serial.print(sc.randomSent);
  Serial.print(" dh=");
  Serial.print(sc.dhKeyReady);
  Serial.print(" chk=");
  Serial.print(sc.checkValuesReady);
  Serial.print(" dhsent=");
  Serial.print(sc.dhKeyCheckSent);
  Serial.print(" rxchk=");
  Serial.print(sc.receivedDhKeyCheckValid);
  Serial.print(" defer=");
  Serial.print(sc.deferredPublicKey);
  Serial.print(sc.deferredConfirm);
  Serial.print(sc.deferredRandom);
  Serial.print(sc.deferredDhKeyCheck);
  Serial.print(" ptx=");
  Serial.print(sc.pendingTxValid);
  Serial.print(" state=");
  Serial.print(sc.pairingState);
  Serial.print(" coop=");
  Serial.print(sc.cooperateHookCount);
  Serial.print(" bg=");
  Serial.print(sc.backgroundServiceCount);
  Serial.print(" p_llid=");
  Serial.print(sc.pendingTxLlid);
  Serial.print(" p_len=");
  Serial.print(sc.pendingTxLength);
  Serial.print(" p_cid=0x");
  Serial.print(sc.pendingTxCid, HEX);
  Serial.print(" p_op=0x");
  Serial.print(sc.pendingTxOpcode, HEX);
  Serial.print(" l_llid=");
  Serial.print(sc.lastTxLlid);
  Serial.print(" l_len=");
  Serial.print(sc.lastTxLength);
  Serial.print(" l_cid=0x");
  Serial.print(sc.lastTxCid, HEX);
  Serial.print(" l_op=0x");
  Serial.print(sc.lastTxOpcode, HEX);
  Serial.print(" fresh=");
  Serial.print(sc.freshTxAllowed);
  Serial.print(" hist=");
  Serial.print(sc.txHistoryValid);
  Serial.print(" txsn=");
  Serial.print(sc.txSn);
  Serial.print(" rxsn=");
  Serial.print(sc.expectedRxSn);
  Serial.print(" p_delay=");
  Serial.println(sc.pendingTxDelayMs);

  BleEncryptionDebugCounters enc{};
  g_ble.getEncryptionDebugCounters(&enc);
  Serial.print("PERIPH enc main_enc_req=");
  Serial.print(enc.mainEncReqSeen);
  Serial.print(" enc_rsp=");
  Serial.print(enc.mainEncRspTxOk);
  Serial.print(" start_req=");
  Serial.print(enc.mainStartEncReqSeen);
  Serial.print(" start_rsp=");
  Serial.print(enc.encStartRspRxCount);
  Serial.print(" mic_fail=");
  Serial.print(enc.encRxMicFailCount);
  Serial.print(" miss=");
  Serial.print(enc.connMissedEventCountLast);
  Serial.print(" miss_max=");
  Serial.print(enc.connMissedEventCountMax);
  Serial.print(" late=");
  Serial.print(enc.connLatePollCount);
  Serial.print(" rx_to=");
  Serial.print(enc.connRxTimeoutCount);
  Serial.print(" bg_due=");
  Serial.println(enc.connBgDueCount);

  BleDisconnectDebug disc{};
  if (g_ble.getDisconnectDebug(&disc) && disc.valid != 0U) {
    Serial.print("PERIPH disc reason=");
    Serial.print(disc.reason);
    Serial.print(" err=0x");
    Serial.print(disc.errorCode, HEX);
    Serial.print(" ev=");
    Serial.print(disc.eventCounter);
    Serial.print(" miss=");
    Serial.print(disc.missedEventCount);
    Serial.print(" ptx=");
    Serial.print(disc.pendingTxValid);
    Serial.print(" plen=");
    Serial.print(disc.pendingTxLength);
    Serial.print(" last_tx=0x");
    Serial.print(disc.lastTxOpcode, HEX);
    Serial.print(" last_rx=0x");
    Serial.print(disc.lastRxOpcode, HEX);
    Serial.print(" rx_len=");
    Serial.println(disc.lastRxLength);
  }
}

void handleCommand(const char* cmd) {
  if (strcmp(cmd, "status") == 0) {
    printStatus();
  } else if (strcmp(cmd, "debug") == 0) {
    printFailureDebug();
  } else if (strcmp(cmd, "clear") == 0) {
    g_ble.clearBondRecord(true);
    Serial.println("PERIPH bond-cleared");
  }
}

void pollCommands() {
  static char buffer[32];
  static uint8_t length = 0U;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      if (length > 0U) {
        buffer[length] = '\0';
        handleCommand(buffer);
        length = 0U;
      }
      continue;
    }
    if ((length + 1U) < sizeof(buffer)) {
      buffer[length++] = c;
    }
  }
}

void onGattWrite(uint16_t handle, const uint8_t* value, uint8_t length,
                 bool withResponse, void*) {
  if (handle != g_writeHandle || value == nullptr) {
    return;
  }
  if (length > sizeof(g_echoBuffer)) {
    length = sizeof(g_echoBuffer);
  }
  memcpy(g_echoBuffer, value, length);
  g_echoLength = length;
  g_echoPending = true;
  ++g_writeCount;

  Serial.print("PERIPH write seq=");
  Serial.print(length > 1U ? g_echoBuffer[1] : 0U);
  Serial.print(" len=");
  Serial.print(length);
  Serial.print(" rsp=");
  Serial.println(withResponse ? 1 : 0);
}

void maybeQueueEcho() {
  if (!g_echoPending || !g_ble.isConnected()) {
    return;
  }
  if (!g_ble.isCustomGattCccdEnabled(g_notifyHandle, false)) {
    return;
  }
  if (!g_ble.setCustomGattCharacteristicValue(g_notifyHandle, g_echoBuffer,
                                              g_echoLength)) {
    return;
  }
  if (!g_ble.notifyCustomGattCharacteristic(g_notifyHandle, false)) {
    return;
  }

  ++g_notifyQueueCount;
  g_echoPending = false;
  Serial.print("PERIPH notify-queued seq=");
  Serial.print(g_echoLength > 1U ? g_echoBuffer[1] : 0U);
  Serial.print(" len=");
  Serial.println(g_echoLength);
}

void maybeRequestSecurity(uint32_t nowMs) {
  if (!kSecure || !g_ble.isConnected() || g_ble.isConnectionEncrypted()) {
    return;
  }
  if ((nowMs - g_lastSecurityRequestMs) < kSecurityRequestRetryMs) {
    return;
  }
  g_lastSecurityRequestMs = nowMs;
  if (g_ble.sendSmpSecurityRequest()) {
    Serial.println("PERIPH security-request");
  }
}

void printVerboseEvent(const BleConnectionEvent& event) {
  if (!kVerbose) {
    return;
  }
  if (event.txPacketSent) {
    Serial.print("PERIPH tx llid=");
    Serial.print(event.txLlid);
    Serial.print(" len=");
    Serial.print(event.txPayloadLength);
    Serial.print(" fresh=");
    Serial.print(event.freshTxAllowed ? 1 : 0);
    Serial.print(" ack=");
    Serial.println(event.peerAckedLastTx ? 1 : 0);
  }
  if (event.packetReceived && event.crcOk) {
    Serial.print("PERIPH rx llid=");
    Serial.print(event.llid);
    Serial.print(" len=");
    Serial.print(event.payloadLength);
    Serial.print(" new=");
    Serial.print(event.packetIsNew ? 1 : 0);
    Serial.print(" ack=");
    Serial.print(event.peerAckedLastTx ? 1 : 0);
    if (event.attPacket && event.payload != nullptr && event.payloadLength > 4U) {
      Serial.print(" att=0x");
      Serial.print(event.payload[4], HEX);
    }
    Serial.println();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialStartMs = millis();
  while (!Serial && ((millis() - serialStartMs) < 1500UL)) {
    delay(1);
  }

  Serial.println();
  Serial.println("PERIPH start BleLinkPayloadRegtestPeripheral");
  Serial.print("PERIPH secure=");
  Serial.println(kSecure ? 1 : 0);

  g_prefs.begin(kPrefsNs, false);
  g_ble.begin(kTxPowerDbm);
  g_ble.loadAddressFromFicr(true);
  g_ble.setBondPersistenceCallbacks(loadBond, saveBond, clearBond, nullptr);
  if (!kSecure) {
    g_ble.clearBondRecord(true);
  }
  g_ble.setPeripheralPreferredAttMtu(247U);
  g_ble.setPeripheralPreferredDataLength(251U);
  g_ble.setPreferredPhyOptions(kBlePhy1M | kBlePhy2M, kBlePhy1M | kBlePhy2M);
  g_ble.setAdvertisingName(kAdvName, true);
  BoardControl::setAntennaPath(BoardAntennaPath::kCeramic);

  if (!g_ble.addCustomGattService(kServiceUuid, &g_serviceHandle) ||
      !g_ble.addCustomGattCharacteristic(
          g_serviceHandle, kNotifyUuid,
          static_cast<uint8_t>(kBleGattPropRead | kBleGattPropNotify),
          reinterpret_cast<const uint8_t*>("ready"), 5U,
          &g_notifyHandle, &g_notifyCccdHandle) ||
      !g_ble.addCustomGattCharacteristic(
          g_serviceHandle, kWriteUuid,
          static_cast<uint8_t>(kBleGattPropWrite | kBleGattPropWriteNoRsp),
          nullptr, 0U, &g_writeHandle)) {
    Serial.println("PERIPH fatal-gatt");
    while (true) {
      delay(1000);
    }
  }

  g_ble.setCustomGattWriteHandler(g_writeHandle, onGattWrite, nullptr);
  g_ble.buildAdvertisingPacket();

  Serial.print("PERIPH handles notify=0x");
  Serial.print(g_notifyHandle, HEX);
  Serial.print(" cccd=0x");
  Serial.print(g_notifyCccdHandle, HEX);
  Serial.print(" write=0x");
  Serial.println(g_writeHandle, HEX);
  Serial.println("PERIPH advertising X54-LINKTEST");
}

void loop() {
  delay(1);
  pollCommands();

  const bool connected = g_ble.isConnected();
  const bool encrypted = g_ble.isConnectionEncrypted();
  const uint32_t nowMs = millis();

  if (connected != g_prevConnected) {
    g_prevConnected = connected;
    if (connected) {
      Serial.println("PERIPH connected");
      g_lastSecurityRequestMs = 0U;
	    } else {
	      Serial.println("PERIPH disconnected");
	      if (kSecure && !g_prevEncrypted) {
	        printFailureDebug();
	      }
	      g_echoPending = false;
	      g_prevEncrypted = false;
	    }
  }

  if (connected && encrypted != g_prevEncrypted) {
    g_prevEncrypted = encrypted;
    Serial.print("PERIPH encrypted=");
    Serial.print(encrypted ? 1 : 0);
    Serial.print(" bond=");
    Serial.println(g_ble.hasBondRecord() ? 1 : 0);
  }

  if (!connected) {
    BleAdvInteraction interaction{};
    (void)g_ble.advertiseInteractEvent(&interaction, 350U, 350000UL,
                                       700000UL);
    return;
  }

  maybeRequestSecurity(nowMs);
  maybeQueueEcho();

  BleConnectionEvent event{};
  if (g_ble.pollConnectionEvent(&event, kPollUs)) {
    printVerboseEvent(event);
  }
}
