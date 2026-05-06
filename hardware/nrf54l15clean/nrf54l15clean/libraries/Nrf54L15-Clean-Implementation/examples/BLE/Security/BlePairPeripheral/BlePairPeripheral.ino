// BLE Pairing & Bonding 2-Board Demo
//
// Board A (PERIPHERAL): Advertises as "X54-PAIR",
//   exposes custom GATT service with notify + write characteristics.
//   Sends SMP Security Request for JustWorks pairing after connect.
//   Stores bond record in Preferences, restores on reboot.
//
// Board B (CENTRAL): Scans for "X54-PAIR", initiates connection,
//   subscribes to notifications, sends periodic writes.
//   Stores bond record in Preferences.
//
// Flash instructions:
//   Board A: set ROLE = PERIPHERAL
//   Board B: set ROLE = CENTRAL
//
// Serial commands:  status, clear

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>
#include "matter_secp256r1.h"
#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

// ═══════════════════════════════════════════════════════════
enum class DemoRole : uint8_t { PERIPHERAL = 0, CENTRAL = 1 };
constexpr DemoRole ROLE = DemoRole::PERIPHERAL;
// ═══════════════════════════════════════════════════════════

namespace {

extern "C" {
extern volatile uint32_t g_ble_periph_rx_trace_count;
extern volatile uint8_t g_ble_periph_rx_trace_llid[32];
extern volatile uint8_t g_ble_periph_rx_trace_len[32];
extern volatile uint8_t g_ble_periph_rx_trace_flags[32];
extern volatile uint8_t g_ble_periph_rx_trace_opcode[32];
extern volatile uint16_t g_ble_periph_rx_trace_cid[32];
extern volatile uint32_t g_ble_periph_tx_trace_count;
extern volatile uint8_t g_ble_periph_tx_trace_llid[32];
extern volatile uint8_t g_ble_periph_tx_trace_len[32];
extern volatile uint8_t g_ble_periph_tx_trace_opcode[32];
extern volatile uint16_t g_ble_periph_tx_trace_cid[32];
}

constexpr char kAdvName[] = "X54-PAIR";
constexpr int8_t kTxPowerDbm = 0;
constexpr uint32_t kStatusMs = 3000U;
constexpr uint32_t kNotifyMs = 4000U;
constexpr uint32_t kPairRetryMs = 3000U;
constexpr uint32_t kConnectionPollUs = 450000UL;
constexpr uint16_t kTestConnIntervalUnits = 24U;
constexpr uint16_t kTestConnTimeoutUnits = 1000U;
#if !defined(BLE_PAIR_ENABLE_PERIPHERAL_NOTIFY)
#define BLE_PAIR_ENABLE_PERIPHERAL_NOTIFY 1
#endif
#if !defined(BLE_PAIR_ENABLE_CENTRAL_SUBSCRIBE)
#define BLE_PAIR_ENABLE_CENTRAL_SUBSCRIBE 1
#endif
#if !defined(BLE_PAIR_ENABLE_CENTRAL_WRITE)
#define BLE_PAIR_ENABLE_CENTRAL_WRITE 1
#endif
#if !defined(BLE_PAIR_CENTRAL_WRITE_WITH_RESPONSE)
#define BLE_PAIR_CENTRAL_WRITE_WITH_RESPONSE 0
#endif
#if !defined(BLE_PAIR_ENABLE_BACKGROUND_CONN_SERVICE)
#define BLE_PAIR_ENABLE_BACKGROUND_CONN_SERVICE 1
#endif
#if !defined(BLE_PAIR_VERBOSE_LINK_LOGS)
#define BLE_PAIR_VERBOSE_LINK_LOGS 0
#endif
#if !defined(BLE_PAIR_QUIET_TEST)
#define BLE_PAIR_QUIET_TEST 0
#endif
constexpr bool kEnablePeripheralNotify =
    (BLE_PAIR_ENABLE_PERIPHERAL_NOTIFY != 0);
constexpr bool kEnableCentralSubscribe =
    (BLE_PAIR_ENABLE_CENTRAL_SUBSCRIBE != 0);
constexpr bool kEnableCentralWrite = (BLE_PAIR_ENABLE_CENTRAL_WRITE != 0);
constexpr bool kCentralWriteWithResponse =
    (BLE_PAIR_CENTRAL_WRITE_WITH_RESPONSE != 0);
constexpr bool kEnableBackgroundConnService =
    (BLE_PAIR_ENABLE_BACKGROUND_CONN_SERVICE != 0);
constexpr bool kVerboseLinkLogs = (BLE_PAIR_VERBOSE_LINK_LOGS != 0);
constexpr bool kQuietTest = (BLE_PAIR_QUIET_TEST != 0);
constexpr uint8_t kTraceBufferDepth = 48U;
constexpr uint8_t kTraceBufferEntryLen = 40U;
constexpr uint16_t kSvcUuid = 0x5A00U;
constexpr uint16_t kNotifyUuid = 0x5A01U;
constexpr uint16_t kWriteUuid = 0x5A02U;
constexpr uint8_t kNotifyProp = 0x12U;  // read + notify
constexpr uint8_t kWriteProp  = 0x04U;  // write without response

BleRadio g_ble;
Preferences g_prefs;
uint16_t g_svcHandle = 0U;
uint16_t g_notifyHandle = 0U;
uint16_t g_notifyCccd = 0U;
uint16_t g_writeHandle = 0U;
uint32_t g_lastStatus = 0U;
uint32_t g_lastNotify = 0U;
uint32_t g_lastScan = 0U;
uint32_t g_notifySeq = 0U;
uint32_t g_lastSecurityRequestMs = 0U;
bool g_prevConnected = false;
bool g_prevEncrypted = false;
bool g_centralSeen = false;
char g_traceBuffer[kTraceBufferDepth][kTraceBufferEntryLen] = {};
uint8_t g_traceBufferHead = 0U;
uint8_t g_traceBufferCount = 0U;

// ─── Bond persistence ─────────────────────────────────────────

static constexpr char kNs[] = "ble_pair";
static constexpr char kBondKey[] = "bond";

bool loadBond(BleBondRecord* out, void*) {
  if (g_prefs.getBytesLength(kBondKey) == sizeof(BleBondRecord)) {
    g_prefs.getBytes(kBondKey, out, sizeof(BleBondRecord));
    Serial.println("ble_pair bond-loaded");
    return true;
  }
  return false;
}

bool saveBond(const BleBondRecord* rec, void*) {
  if (!rec) return false;
  g_prefs.putBytes(kBondKey, rec, sizeof(BleBondRecord));
  Serial.println("ble_pair bond-saved");
  return true;
}

bool clearBondStored(void*) {
  g_prefs.remove(kBondKey);
  Serial.println("ble_pair bond-cleared-storage");
  return true;
}

// ─── Peripheral: GATT write handler ──────────────────────────

void onGattWrite(uint16_t h, const uint8_t* d, uint8_t n, bool, void*) {
  if (h != g_writeHandle || !d) return;
  Serial.print("ble_pair gatt-write val=");
  if (n == 1U) {
    Serial.println(d[0] ? "ON" : "OFF");
#if defined(LED_BUILTIN)
    digitalWrite(LED_BUILTIN, d[0] ? HIGH : LOW);
#endif
  } else {
    Serial.write(d, n < 16U ? n : 16U);
    Serial.println();
  }
}

// ─── Helpers ─────────────────────────────────────────────────

void printAddr(const uint8_t a[6]) {
  for (int i = 0; i < 6; i++) {
    if (a[i] < 16) Serial.print('0');
    Serial.print(a[i], HEX);
    if (i < 5) Serial.print(':');
  }
}

void printStatus() {
  const bool connected = g_ble.isConnected();
  const bool encrypted = g_ble.isConnectionEncrypted();
  const bool bonded = g_ble.hasBondRecord();

  Serial.print("ble_pair role=");
  Serial.print(ROLE==DemoRole::PERIPHERAL?"periph":"central");
  Serial.print(" conn=");
  Serial.print(connected?1:0);
  Serial.print(" enc=");
  Serial.print(encrypted?1:0);
  Serial.print(" bond=");
  Serial.print(bonded?1:0);
  Serial.print(" notif=");
  Serial.print(g_notifySeq);

  BleEncryptionDebugCounters dbg;
  g_ble.getEncryptionDebugCounters(&dbg);
  Serial.print(" mic=");
  Serial.print(dbg.encRxMicFailCount);

  BleConnectionInfo info;
  if (g_ble.getConnectionInfo(&info)) {
    int8_t rssi = 0;
    g_ble.getLatestConnectionRssiDbm(&rssi);
    Serial.print(" rssi=");
    Serial.print(rssi);
    Serial.print("dBm peer=");
    printAddr(info.peerAddress);
    if (info.peerAddressRandom) Serial.print("(rnd)");
  }
  Serial.println();
}

void printHexBytes(const uint8_t* data, size_t len) {
  if (data == nullptr) {
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    if (data[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
}

void printHex64(uint64_t value) {
  for (int i = 15; i >= 0; --i) {
    const uint8_t nibble = static_cast<uint8_t>((value >> (i * 4)) & 0x0FULL);
    Serial.print(static_cast<char>((nibble < 10U) ? ('0' + nibble)
                                                  : ('A' + (nibble - 10U))));
  }
}

void printScDebug() {
  BleSecureConnectionsDebugState dbg{};
  g_ble.getSecureConnectionsDebugState(&dbg);
  Serial.print("sc_dbg active=");
  Serial.print(dbg.active);
  Serial.print(" local_init=");
  Serial.print(dbg.localInitiator);
  Serial.print(" peer_pub=");
  Serial.print(dbg.peerPublicKeyValid);
  Serial.print(" pub_tx=");
  Serial.print(dbg.publicKeySent);
  Serial.print(" conf_tx=");
  Serial.print(dbg.confirmSent);
  Serial.print(" rand_tx=");
  Serial.print(dbg.randomSent);
  Serial.print(" dh_ready=");
  Serial.print(dbg.dhKeyReady);
  Serial.print(" check_ready=");
  Serial.print(dbg.checkValuesReady);
  Serial.print(" dhcheck_tx=");
  Serial.print(dbg.dhKeyCheckSent);
  Serial.print(" dhcheck_rx=");
  Serial.print(dbg.receivedDhKeyCheckValid);
  Serial.print(" def_pub=");
  Serial.print(dbg.deferredPublicKey);
  Serial.print(" def_conf=");
  Serial.print(dbg.deferredConfirm);
  Serial.print(" def_rand=");
  Serial.print(dbg.deferredRandom);
  Serial.print(" def_dh=");
  Serial.print(dbg.deferredDhKeyCheck);
  Serial.print(" pend_tx=");
  Serial.print(dbg.pendingTxValid);
  Serial.print(" state=");
  Serial.print(dbg.pairingState);
  Serial.print(" key_us=");
  Serial.print(dbg.localKeypairTimeUs);
  Serial.print(" dh_us=");
  Serial.print(dbg.dhKeyTimeUs);
  Serial.print(" chk_us=");
  Serial.print(dbg.checkValuesTimeUs);
  Serial.print(" coop=");
  Serial.print(dbg.cooperateHookCount);
  Serial.print(" bg=");
  Serial.print(dbg.backgroundServiceCount);
  Serial.print(" ldh=");
  printHexBytes(dbg.localDhKeyCheck, sizeof(dbg.localDhKeyCheck));
  Serial.print(" pdh=");
  printHexBytes(dbg.peerDhKeyCheck, sizeof(dbg.peerDhKeyCheck));
  Serial.print(" rdh=");
  printHexBytes(dbg.receivedDhKeyCheck, sizeof(dbg.receivedDhKeyCheck));
  Serial.println();
}

void printDisconnectDebug() {
  BleDisconnectDebug dbg{};
  if (!g_ble.getDisconnectDebug(&dbg)) {
    Serial.println("disc_dbg none");
    return;
  }
  Serial.print("disc_dbg seq=");
  Serial.print(dbg.sequence);
  Serial.print(" reason=");
  Serial.print(dbg.reason);
  Serial.print(" err=0x");
  if (dbg.errorCode < 16U) Serial.print('0');
  Serial.print(dbg.errorCode, HEX);
  Serial.print(" ce=");
  Serial.print(dbg.eventCounter);
  Serial.print(" missed=");
  Serial.print(dbg.missedEventCount);
  Serial.print(" pend=");
  Serial.print(dbg.pendingTxValid);
  Serial.print(" pll=");
  Serial.print(dbg.pendingTxLlid);
  Serial.print(" plen=");
  Serial.print(dbg.pendingTxLength);
  Serial.print(" lasttx_op=0x");
  if (dbg.lastTxOpcode < 16U) Serial.print('0');
  Serial.print(dbg.lastTxOpcode, HEX);
  Serial.print(" lastrx_op=0x");
  if (dbg.lastRxOpcode < 16U) Serial.print('0');
  Serial.print(dbg.lastRxOpcode, HEX);
  Serial.print(" ack=");
  Serial.print(dbg.lastPeerAckedLastTx);
  Serial.println();
}

void printControllerTraceRing() {
  const uint32_t rxCount = g_ble_periph_rx_trace_count;
  const uint32_t txCount = g_ble_periph_tx_trace_count;
  Serial.print("trace_dbg rx_count=");
  Serial.print(rxCount);
  Serial.print(" tx_count=");
  Serial.println(txCount);

  const uint8_t rxDump = (rxCount > 8U) ? 8U : static_cast<uint8_t>(rxCount);
  const uint8_t rxStart =
      (rxCount > rxDump)
          ? static_cast<uint8_t>((rxCount - rxDump) & 0x1FU)
          : 0U;
  for (uint8_t i = 0U; i < rxDump; ++i) {
    const uint8_t slot = static_cast<uint8_t>((rxStart + i) & 0x1FU);
    Serial.print("trace_rx[");
    Serial.print(i);
    Serial.print("] llid=");
    Serial.print(g_ble_periph_rx_trace_llid[slot]);
    Serial.print(" len=");
    Serial.print(g_ble_periph_rx_trace_len[slot]);
    Serial.print(" flags=0x");
    if (g_ble_periph_rx_trace_flags[slot] < 16U) Serial.print('0');
    Serial.print(g_ble_periph_rx_trace_flags[slot], HEX);
    Serial.print(" cid=0x");
    if (g_ble_periph_rx_trace_cid[slot] < 0x1000U) Serial.print('0');
    if (g_ble_periph_rx_trace_cid[slot] < 0x0100U) Serial.print('0');
    if (g_ble_periph_rx_trace_cid[slot] < 0x0010U) Serial.print('0');
    Serial.print(g_ble_periph_rx_trace_cid[slot], HEX);
    Serial.print(" op=0x");
    if (g_ble_periph_rx_trace_opcode[slot] < 16U) Serial.print('0');
    Serial.println(g_ble_periph_rx_trace_opcode[slot], HEX);
  }

  const uint8_t txDump = (txCount > 8U) ? 8U : static_cast<uint8_t>(txCount);
  const uint8_t txStart =
      (txCount > txDump)
          ? static_cast<uint8_t>((txCount - txDump) & 0x1FU)
          : 0U;
  for (uint8_t i = 0U; i < txDump; ++i) {
    const uint8_t slot = static_cast<uint8_t>((txStart + i) & 0x1FU);
    Serial.print("trace_tx[");
    Serial.print(i);
    Serial.print("] llid=");
    Serial.print(g_ble_periph_tx_trace_llid[slot]);
    Serial.print(" len=");
    Serial.print(g_ble_periph_tx_trace_len[slot]);
    Serial.print(" cid=0x");
    if (g_ble_periph_tx_trace_cid[slot] < 0x1000U) Serial.print('0');
    if (g_ble_periph_tx_trace_cid[slot] < 0x0100U) Serial.print('0');
    if (g_ble_periph_tx_trace_cid[slot] < 0x0010U) Serial.print('0');
    Serial.print(g_ble_periph_tx_trace_cid[slot], HEX);
    Serial.print(" op=0x");
    if (g_ble_periph_tx_trace_opcode[slot] < 16U) Serial.print('0');
    Serial.println(g_ble_periph_tx_trace_opcode[slot], HEX);
  }
}

void printEncDiag() {
  BleEncryptionDebugCounters dbg{};
  g_ble.getEncryptionDebugCounters(&dbg);
  Serial.print("enc_dbg main_enc=");
  Serial.print(dbg.mainEncReqSeen);
  Serial.print(" main_start=");
  Serial.print(dbg.mainStartEncReqSeen);
  Serial.print(" main_start_dec=");
  Serial.print(dbg.mainStartEncReqSeenDecrypted);
  Serial.print(" main_enc_rsp_tx=");
  Serial.print(dbg.mainEncRspTxOk);
  Serial.print(" main_start_rsp_tx=");
  Serial.print(dbg.mainStartEncRspTxOk);
  Serial.print(" follow_start_rsp_tx=");
  Serial.print(dbg.followupStartEncRspTxOk);
  Serial.print(" start_pending_rx=");
  Serial.print(dbg.startPendingControlRxSeen);
  Serial.print(" start_pending_byte0=0x");
  if (dbg.startPendingLastByte0 < 16U) Serial.print('0');
  Serial.print(dbg.startPendingLastByte0, HEX);
  Serial.print(" start_pending_raw=");
  Serial.print(dbg.startPendingLastLenRaw);
  Serial.print(" start_pending_dec=");
  Serial.print(dbg.startPendingLastDecrypted);
  Serial.print(" start_rsp_rx=");
  Serial.print(dbg.encStartRspRxCount);
  Serial.print(" start_rsp_raw=");
  Serial.print(dbg.encStartRspLastRawLen);
  Serial.print(" start_rsp_dec=");
  Serial.print(dbg.encStartRspLastDecrypted);
  Serial.print(" mic=");
  Serial.print(dbg.encRxMicFailCount);
  Serial.print(" short=");
  Serial.print(dbg.encRxShortPduCount);
  Serial.print(" txpkt=");
  Serial.print(dbg.encTxPacketCount);
  Serial.print(" txctr=");
  Serial.print(dbg.encLastTxCounterLo);
  Serial.print(" rxctr=");
  Serial.print(dbg.encLastRxCounterLo);
  Serial.print(" rxdir=");
  Serial.print(dbg.encLastRxDir);
  Serial.print(" txdir=");
  Serial.print(dbg.encLastTxDir);
  Serial.print(" sk=");
  Serial.print(dbg.encLastSessionKeyValid);
  Serial.print(" alt=");
  Serial.print(dbg.encLastSessionAltKeyValid);
  Serial.print(" tx_to=");
  Serial.print(dbg.connTxTimeoutCount);
  Serial.print(" rx_to=");
  Serial.print(dbg.connRxTimeoutCount);
  Serial.print(" f_rx_to=");
  Serial.print(dbg.connFollowupRxTimeoutCount);
  Serial.print(" f_end=");
  Serial.print(dbg.followupEndSeen);
  Serial.print(" f_crc=");
  Serial.print(dbg.followupCrcOk);
  Serial.print(" f_llid=");
  Serial.print(dbg.lastFollowLlid);
  Serial.print(" f_len=");
  Serial.print(dbg.lastFollowLen);
  Serial.print(" f_b0=0x");
  if (dbg.lastFollowByte0 < 16U) Serial.print('0');
  Serial.print(dbg.lastFollowByte0, HEX);
  Serial.print(" tx_plain=");
  Serial.print(dbg.encLastTxPlainLen);
  Serial.print(" tx_air=");
  Serial.print(dbg.encLastTxAirLen);
  Serial.print(" tx_fresh=");
  Serial.print(dbg.encLastTxWasFresh);
  Serial.print(" tx_enc=");
  Serial.print(dbg.encLastTxWasEncrypted);
  Serial.println();
}

void appendTraceBuffer(const char* message) {
  if (message == nullptr) {
    return;
  }
  char* slot = g_traceBuffer[g_traceBufferHead];
  size_t i = 0U;
  for (; i < (kTraceBufferEntryLen - 1U) && message[i] != '\0'; ++i) {
    slot[i] = message[i];
  }
  slot[i] = '\0';
  g_traceBufferHead =
      static_cast<uint8_t>((g_traceBufferHead + 1U) % kTraceBufferDepth);
  if (g_traceBufferCount < kTraceBufferDepth) {
    ++g_traceBufferCount;
  }
}

void dumpTraceBuffer() {
  Serial.print("trace_dump count=");
  Serial.println(g_traceBufferCount);
  const uint8_t start =
      static_cast<uint8_t>((g_traceBufferHead + kTraceBufferDepth -
                            g_traceBufferCount) %
                           kTraceBufferDepth);
  for (uint8_t i = 0U; i < g_traceBufferCount; ++i) {
    const uint8_t index =
        static_cast<uint8_t>((start + i) % kTraceBufferDepth);
    Serial.print("trace[");
    Serial.print(i);
    Serial.print("]=");
    Serial.println(g_traceBuffer[index]);
  }
}

void onBleTrace(const char* message, void*) { appendTraceBuffer(message); }

void printKeyProbe() {
  Secp256r1Scalar priv{};
  Secp256r1Point pub{};
  Secp256r1::generateKeyPair(&priv, &pub);
  Serial.print("keyprobe uid64=");
  printHex64(hardwareUniqueId64());
  Serial.print(" eui64=");
  printHex64(zigbeeFactoryEui64());
  Serial.print(" priv=");
  printHexBytes(priv.bytes, sizeof(priv.bytes));
  Serial.print(" pubx=");
  printHexBytes(pub.x, sizeof(pub.x));
  Serial.println();
}

uint16_t readLe16Local(const uint8_t* p) {
  if (p == nullptr) {
    return 0U;
  }
  return static_cast<uint16_t>(p[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8U);
}

void logSmpEvent(const BleConnectionEvent& evt) {
  if (!kVerboseLinkLogs) {
    return;
  }
  bool printed = false;
  if ((evt.llid == 0x02U) && (evt.payloadLength >= 5U) &&
      (evt.payload != nullptr) &&
      (readLe16Local(&evt.payload[2]) == 0x0006U)) {
    Serial.print("smp_rx ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" code=0x");
    const uint8_t code = evt.payload[4];
    if (code < 16U) Serial.print('0');
    Serial.print(code, HEX);
    Serial.print(" bytes=");
    const uint8_t dumpLen =
        (evt.payloadLength < 11U) ? evt.payloadLength : 11U;
    for (uint8_t i = 4U; i < dumpLen; ++i) {
      if (evt.payload[i] < 16U) Serial.print('0');
      Serial.print(evt.payload[i], HEX);
    }
    printed = true;
  }
  if ((evt.txLlid == 0x02U) && (evt.txPayloadLength >= 5U) &&
      (evt.txPayload != nullptr) &&
      (readLe16Local(&evt.txPayload[2]) == 0x0006U)) {
    if (printed) Serial.print(' ');
    Serial.print("smp_tx ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" code=0x");
    const uint8_t code = evt.txPayload[4];
    if (code < 16U) Serial.print('0');
    Serial.print(code, HEX);
    Serial.print(" bytes=");
    const uint8_t dumpLen =
        (evt.txPayloadLength < 11U) ? evt.txPayloadLength : 11U;
    for (uint8_t i = 4U; i < dumpLen; ++i) {
      if (evt.txPayload[i] < 16U) Serial.print('0');
      Serial.print(evt.txPayload[i], HEX);
    }
    printed = true;
  }
  if (printed) {
    Serial.println();
  }

  auto isSecurityLlOpcode = [](uint8_t opcode) {
    return opcode == 0x03U || opcode == 0x04U ||
           opcode == 0x05U || opcode == 0x06U;
  };

  bool llPrinted = false;
  if (evt.packetReceived && (evt.llid == 0x03U) &&
      (evt.payload != nullptr) && (evt.payloadLength >= 1U)) {
    Serial.print("ll_rx ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" op=0x");
    const uint8_t rxOpcode = evt.llControlPacket ? evt.llControlOpcode : evt.payload[0];
    if (rxOpcode < 16U) Serial.print('0');
    Serial.print(rxOpcode, HEX);
    Serial.print(" new=");
    Serial.print(evt.packetIsNew ? 1 : 0);
    Serial.print(" sn=");
    Serial.print(evt.rxSn);
    Serial.print(" nesn=");
    Serial.print(evt.rxNesn);
    Serial.print(" len=");
    Serial.print(evt.payloadLength);
    Serial.print(" ctrl=");
    Serial.print(evt.llControlPacket ? 1 : 0);
    llPrinted = true;
  }
  if ((evt.txLlid == 0x03U) &&
      (evt.txPayloadLength >= 1U) &&
      (evt.txPayload != nullptr) &&
      isSecurityLlOpcode(evt.txPayload[0])) {
    if (llPrinted) Serial.print(' ');
    Serial.print("ll_tx ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" op=0x");
    const uint8_t opcode = evt.txPayload[0];
    if (opcode < 16U) Serial.print('0');
    Serial.print(opcode, HEX);
    llPrinted = true;
  }
  if (llPrinted) {
    Serial.println();
  }

  auto isInterestingAttOpcode = [](uint8_t opcode) {
    return opcode == 0x01U ||  // Error Response
           opcode == 0x12U ||  // Write Request
           opcode == 0x13U ||  // Write Response
           opcode == 0x1BU ||  // Handle Value Notification
           opcode == 0x1DU;    // Handle Value Indication
  };

  bool attPrinted = false;
  if (evt.attPacket && isInterestingAttOpcode(evt.attOpcode)) {
    Serial.print("att_rx ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" op=0x");
    if (evt.attOpcode < 16U) Serial.print('0');
    Serial.print(evt.attOpcode, HEX);
    attPrinted = true;
  }
  if ((evt.txLlid == 0x02U) &&
      (evt.txPayloadLength >= 5U) &&
      (evt.txPayload != nullptr) &&
      (readLe16Local(&evt.txPayload[2]) == 0x0004U)) {
    const uint8_t attOpcode = evt.txPayload[4];
    if (isInterestingAttOpcode(attOpcode)) {
      if (attPrinted) Serial.print(' ');
      Serial.print("att_tx ce=");
      Serial.print(evt.eventCounter);
      Serial.print(" op=0x");
      if (attOpcode < 16U) Serial.print('0');
      Serial.print(attOpcode, HEX);
      attPrinted = true;
    }
  }
  if (attPrinted) {
    Serial.println();
  }
}

void handleCmd(const char* c) {
  if (strcmp(c,"status")==0) { printStatus(); return; }
  if (strcmp(c,"sc")==0) { printScDebug(); return; }
  if (strcmp(c,"enc")==0) { printEncDiag(); return; }
  if (strcmp(c,"disc")==0) { printDisconnectDebug(); return; }
  if (strcmp(c,"trace")==0) { dumpTraceBuffer(); return; }
  if (strcmp(c,"keyprobe")==0) { printKeyProbe(); return; }
  if (strcmp(c,"clear")==0) {
    g_ble.clearBondRecord(true);
    Serial.println("ble_pair bond-cleared");
    return;
  }
  Serial.print("ble_pair ? ");
  Serial.println(c);
}

void pollCmds() {
  static char b[32]; static size_t l=0;
  while (Serial.available()) {
    char c=Serial.read();
    if (c=='\r'||c=='\n') { if(l){b[l]=0;handleCmd(b);l=0;memset(b,0,32);} }
    else if(l+1<32) b[l++]=c;
  }
}

// ─── Central: scan helper ────────────────────────────────────

bool scanForPeer(uint8_t peerAddr[6], bool* addrRandom) {
  BleActiveScanResult r;
  if (g_ble.scanActiveCycle(&r, 300000UL, 300000UL)) {
    memcpy(peerAddr, r.advertiserAddress, 6);
    *addrRandom = r.advertiserAddressRandom;
    return true;
  }
  return false;
}

}  // namespace

// ─── Setup ──────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  { uint32_t t=millis(); while(!Serial&&(millis()-t)<1500U){} }

#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif

  Serial.println();
  Serial.print("ble_pair === BLE Pairing Demo (");
  Serial.print(ROLE==DemoRole::PERIPHERAL?"PERIPHERAL":"CENTRAL");
  Serial.println(") ===");

  g_prefs.begin(kNs, false);

  if (!g_ble.begin(kTxPowerDbm)) {
    Serial.println("ble_pair FATAL: radio init failed");
    return;
  }
  g_ble.setBackgroundConnectionServiceEnabled(kEnableBackgroundConnService);
  g_ble.setTraceCallback(onBleTrace, nullptr);
  g_ble.loadAddressFromFicr(true);
  g_ble.setBondPersistenceCallbacks(loadBond, saveBond, clearBondStored, nullptr);

  if (ROLE == DemoRole::PERIPHERAL) {
    g_ble.setAdvertisingName(kAdvName, true);

    if (!g_ble.addCustomGattService(kSvcUuid, &g_svcHandle)) {
      Serial.println("ble_pair FATAL: svc failed"); return;
    }
    uint8_t initVal[4]={'i','n','i','t'};
    if (!g_ble.addCustomGattCharacteristic(g_svcHandle, kNotifyUuid,
                                           kNotifyProp, initVal, 4,
                                           &g_notifyHandle, &g_notifyCccd)) {
      Serial.println("ble_pair FATAL: notify char failed"); return;
    }
    if (!g_ble.addCustomGattCharacteristic(g_svcHandle, kWriteUuid,
                                           kWriteProp, nullptr, 0,
                                           &g_writeHandle)) {
      Serial.println("ble_pair FATAL: write char failed"); return;
    }
    g_ble.setCustomGattWriteHandler(g_writeHandle, onGattWrite, nullptr);
    g_ble.buildAdvertisingPacket();
    Serial.println("ble_pair gatt+adv: ready");
  }
  printStatus();
  Serial.println("ble_pair cmd: status clear sc enc disc trace keyprobe");
}

// ─── Loop ───────────────────────────────────────────────────

void loop() {
  // BLE runs via ISR callbacks - no polling needed
  delay(1);
  pollCmds();

  const bool connected = g_ble.isConnected();
  const bool encrypted = g_ble.isConnectionEncrypted();

  // ─── Peripheral ──────────────────────────────────────────
  if (ROLE == DemoRole::PERIPHERAL) {
    // Connection state tracking
    if (connected != g_prevConnected) {
      g_prevConnected = connected;
      if (connected) {
        Serial.println("ble_pair connected");
        // If no bond, request pairing
        if (!g_ble.hasBondRecord()) {
          delay(50);
          const bool queued = g_ble.sendSmpSecurityRequest();
          g_lastSecurityRequestMs = millis();
          Serial.print("ble_pair sent-smp-security-request=");
          Serial.println(queued ? "1" : "0");
        }
      } else {
        Serial.println("ble_pair disconnected");
        printEncDiag();
        printControllerTraceRing();
        printDisconnectDebug();
        g_ble.clearEncryptionDebugCounters();
      }
    }

    // Encryption state tracking
    if (connected && encrypted != g_prevEncrypted) {
      g_prevEncrypted = encrypted;
      Serial.print("ble_pair encryption=");
      Serial.println(encrypted ? "ON" : "OFF");
    }

    if (!connected) {
      // Advertise
      BleAdvInteraction adv;
      g_ble.advertiseInteractEvent(&adv, 350U, 350000UL, 700000UL);
    } else {
      if (!encrypted && !g_ble.hasBondRecord() &&
          (millis() - g_lastSecurityRequestMs) >= kPairRetryMs) {
        const bool queued = g_ble.sendSmpSecurityRequest();
        g_lastSecurityRequestMs = millis();
        Serial.print("ble_pair retry-smp-security-request=");
        Serial.println(queued ? "1" : "0");
      }
      // Process connection events
      BleConnectionEvent evt;
      (void)g_ble.pollConnectionEvent(&evt, kConnectionPollUs);
      logSmpEvent(evt);

      // Notify periodically when encrypted + subscribed
      if (kEnablePeripheralNotify &&
          encrypted && g_ble.isCustomGattCccdEnabled(g_notifyHandle) &&
          (millis() - g_lastNotify) >= kNotifyMs) {
        g_lastNotify = millis();
        const uint32_t nextSeq = g_notifySeq + 1U;
        char buf[32];
        snprintf(buf, sizeof(buf), "ping-%lu", (unsigned long)nextSeq);
        g_ble.setCustomGattCharacteristicValue(g_notifyHandle,
                                               (const uint8_t*)buf, strlen(buf));
        const bool queued = g_ble.notifyCustomGattCharacteristic(g_notifyHandle);
        const bool stillQueued =
            g_ble.isCustomGattNotificationQueued(g_notifyHandle, false);
        if (!kQuietTest) {
          Serial.print("ble_pair notify_try seq=");
          Serial.print(nextSeq);
          Serial.print(" queued=");
          Serial.print(queued ? 1 : 0);
          Serial.print(" pending=");
          Serial.println(stillQueued ? 1 : 0);
        }
        if (queued) {
          g_notifySeq = nextSeq;
        }
      }
    }
  }

  // ─── Central ─────────────────────────────────────────────
  if (ROLE == DemoRole::CENTRAL) {
    if (connected != g_prevConnected) {
      g_prevConnected = connected;
      if (connected) {
        Serial.println("ble_pair central: connected");
        g_centralSeen = true;
        g_ble.setPreferredConnectionParameters(kTestConnIntervalUnits,
                                               kTestConnIntervalUnits,
                                               0U,
                                               kTestConnTimeoutUnits);
      } else {
        Serial.println("ble_pair central: disconnected");
        printEncDiag();
        printDisconnectDebug();
        g_centralSeen = false;
        g_ble.clearEncryptionDebugCounters();
      }
    }

    if (connected && encrypted != g_prevEncrypted) {
      g_prevEncrypted = encrypted;
      Serial.print("ble_pair central: encryption=");
      Serial.println(encrypted ? "ON" : "OFF");
    }

    if (!connected) {
      // Scan and connect
      if ((millis() - g_lastScan) >= 500U) {
        g_lastScan = millis();
        BleActiveScanResult r;
        if (g_ble.scanActiveCycle(&r, 300000UL, 300000UL)) {
          Serial.print("ble_pair central: found rssi=");
          Serial.print(r.advRssiDbm);
          Serial.print("dBm connecting... ");
          if (g_ble.initiateConnection(r.advertiserAddress,
                                       r.advertiserAddressRandom,
                                       kTestConnIntervalUnits,
                                       kTestConnTimeoutUnits,
                                       9U, 300000UL)) {
            Serial.println("OK");
          } else {
            Serial.println("FAIL");
          }
        }
      }
    } else {
      // Process events
      BleConnectionEvent evt;
      (void)g_ble.pollConnectionEvent(&evt, kConnectionPollUs);
      logSmpEvent(evt);

      // Once encrypted, subscribe to notifications
      static bool subbed = false;
      if (kEnableCentralSubscribe && encrypted && !subbed) {
        g_ble.queueAttCccdWrite(g_notifyCccd, true, false, true);
        subbed = true;
        Serial.println("ble_pair central: subscribed");
      }
      if (!encrypted || !kEnableCentralSubscribe) subbed = false;

      // Periodic write
      static uint32_t lastWr = 0;
      if (kEnableCentralWrite &&
          encrypted && (millis() - lastWr) >= 5000U) {
        lastWr = millis();
        static bool on = false; on = !on;
        const uint8_t v = on ? 1U : 0U;
        g_ble.queueAttWriteRequest(g_writeHandle, &v, 1U,
                                   kCentralWriteWithResponse);
        Serial.print("ble_pair central: wrote ");
        Serial.println(on ? "ON" : "OFF");
      }

      // Read notifications
      uint8_t nbuf[32]={0}; uint8_t nlen=32;
      if (g_ble.getCustomGattCharacteristicValue(g_notifyHandle, nbuf, &nlen) && nlen) {
        Serial.print("ble_pair central: notify=");
        Serial.write(nbuf, nlen<30?nlen:30);
        Serial.println();
      }
    }
  }

  // Status
  if (!kQuietTest && (millis() - g_lastStatus) >= kStatusMs) {
    g_lastStatus = millis();
    printStatus();
  }
}
