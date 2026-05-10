/*
  BLE link payload regression central

  Flash BleLinkPayloadRegtestPeripheral to one nRF54L15 board and this sketch
  to the other board. By default the link is plain/insecure. For LE Secure
  Connections testing, compile both sketches with:

    -DBLE_LINK_REGTEST_SECURE=1

  The central connects to X54-LINKTEST, optionally waits for encryption,
  negotiates MTU/DLE/PHY, subscribes to notifications, then writes payloads at
  critical sizes. The peripheral echoes each payload back as a notification.

  Serial commands at 115200 baud:
    status  - print link state
    results - print pass/fail counters
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

struct TestCase {
  const char* name;
  uint16_t mtu;
  uint16_t dle;
  uint8_t phy;
};

const TestCase kTests[] = {
    {"MTU23_DLE27_1M", 23U, 27U, kBlePhy1M},
    {"MTU64_DLE27_1M", 64U, 27U, kBlePhy1M},
    {"MTU128_DLE64_1M", 128U, 64U, kBlePhy1M},
    {"MTU247_DLE64_1M", 247U, 64U, kBlePhy1M},
    {"MTU247_DLE251_1M", 247U, 251U, kBlePhy1M},
    {"MTU247_DLE251_2M", 247U, 251U, kBlePhy2M},
};

BleRadio g_ble;
Preferences g_prefs;

constexpr bool kSecure = (BLE_LINK_REGTEST_SECURE != 0);
constexpr bool kVerbose = (BLE_LINK_REGTEST_VERBOSE != 0);
constexpr char kTargetName[] = "X54-LINKTEST";
constexpr int8_t kTxPowerDbm = 0;
constexpr uint32_t kScanUs = 350000UL;
constexpr uint32_t kPollUs = 450000UL;
constexpr uint32_t kPhaseTimeoutMs = kSecure ? 45000UL : 30000UL;
constexpr uint16_t kConnIntervalUnits = 24U;
constexpr uint16_t kConnTimeoutUnits = 1000U;
constexpr uint16_t kPeerNotifyHandle = 0x0022U;
constexpr uint16_t kPeerNotifyCccdHandle = 0x0023U;
constexpr uint16_t kPeerWriteHandle = 0x0025U;
constexpr char kPrefsNs[] = "ble_link_reg";
constexpr char kBondKey[] = "bond";

enum class Phase : uint8_t {
  kConnect = 0,
  kWaitSecurity,
  kNegotiate,
  kSubscribe,
  kPayload,
  kDisconnect,
  kWaitNext,
};

Phase g_phase = Phase::kConnect;
uint8_t g_testIndex = 0U;
uint8_t g_payloadIndex = 0U;
uint8_t g_sequence = 1U;
uint32_t g_phaseStartMs = 0UL;
uint32_t g_cycleCount = 0UL;
uint32_t g_passCount = 0UL;
uint32_t g_failCount = 0UL;
bool g_prevConnected = false;
bool g_mtuRequested = false;
bool g_dleRequested = false;
bool g_phyRequested = false;
bool g_subscribeQueued = false;
bool g_expectNotification = false;
bool g_writeQueued = false;
uint8_t g_expectedPayload[BleRadio::kCustomGattMaxValueLength];
uint8_t g_expectedLength = 0U;

uint16_t readLe16Local(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8U);
}

uint16_t minU16Local(uint16_t a, uint16_t b) {
  return (a < b) ? a : b;
}

uint8_t maxU8Local(uint8_t a, uint8_t b) {
  return (a > b) ? a : b;
}

bool loadBond(BleBondRecord* outRecord, void*) {
  if (!kSecure ||
      outRecord == nullptr ||
      g_prefs.getBytesLength(kBondKey) != sizeof(BleBondRecord)) {
    return false;
  }
  g_prefs.getBytes(kBondKey, outRecord, sizeof(BleBondRecord));
  Serial.println("CENTRAL bond-loaded");
  return true;
}

bool saveBond(const BleBondRecord* record, void*) {
  if (!kSecure || record == nullptr) {
    return false;
  }
  g_prefs.putBytes(kBondKey, record, sizeof(BleBondRecord));
  Serial.println("CENTRAL bond-saved");
  return true;
}

bool clearBond(void*) {
  g_prefs.remove(kBondKey);
  return true;
}

uint8_t currentMaxValueLength() {
  const uint16_t mtu = g_ble.currentAttMtu();
  if (mtu <= 3U) {
    return 1U;
  }
  uint16_t maxLen = static_cast<uint16_t>(mtu - 3U);
  if (maxLen > BleRadio::kCustomGattMaxValueLength) {
    maxLen = BleRadio::kCustomGattMaxValueLength;
  }
  return static_cast<uint8_t>(maxLen);
}

uint8_t payloadLengthForIndex(uint8_t index) {
  const uint8_t maxLen = currentMaxValueLength();
  const uint8_t halfLen = maxU8Local(1U, static_cast<uint8_t>(maxLen / 2U));
  const uint8_t boundaryLen =
      (g_ble.currentDataLength() > 7U)
          ? static_cast<uint8_t>(minU16Local(
                maxLen, static_cast<uint16_t>(g_ble.currentDataLength() - 7U)))
          : 1U;
  const uint8_t afterBoundaryLen =
      static_cast<uint8_t>(minU16Local(
          maxLen, static_cast<uint16_t>(boundaryLen + 1U)));
  const uint8_t candidates[] = {
      1U,
      static_cast<uint8_t>(minU16Local(maxLen, 2U)),
      static_cast<uint8_t>(minU16Local(maxLen, 20U)),
      boundaryLen,
      afterBoundaryLen,
      halfLen,
      maxLen,
  };

  uint8_t uniqueIndex = 0U;
  for (uint8_t i = 0U; i < sizeof(candidates); ++i) {
    const uint8_t candidate = candidates[i];
    bool seen = false;
    for (uint8_t j = 0U; j < i; ++j) {
      if (candidates[j] == candidate) {
        seen = true;
        break;
      }
    }
    if (candidate == 0U || seen) {
      continue;
    }
    if (uniqueIndex == index) {
      return candidate;
    }
    ++uniqueIndex;
  }
  return 0U;
}

void buildExpectedPayload(uint8_t length) {
  g_expectedLength = length;
  for (uint8_t i = 0U; i < length; ++i) {
    g_expectedPayload[i] =
        static_cast<uint8_t>(0x31U + ((g_sequence + i * 17U) & 0x3FU));
  }
  if (length > 0U) {
    g_expectedPayload[0] = 0xA5U;
  }
  if (length > 1U) {
    g_expectedPayload[1] = g_sequence;
  }
  if (length > 2U) {
    g_expectedPayload[2] = length;
  }
}

bool advertisingDataHasName(const uint8_t* data, uint8_t length) {
  uint8_t offset = 0U;
  while (offset < length) {
    const uint8_t fieldLen = data[offset];
    if (fieldLen == 0U) {
      return false;
    }
    if (static_cast<uint16_t>(offset + fieldLen) >= length) {
      return false;
    }
    const uint8_t type = data[offset + 1U];
    if (type == 0x08U || type == 0x09U) {
      const uint8_t nameLen = static_cast<uint8_t>(fieldLen - 1U);
      if (nameLen == (sizeof(kTargetName) - 1U) &&
          memcmp(&data[offset + 2U], kTargetName, nameLen) == 0) {
        return true;
      }
    }
    offset = static_cast<uint8_t>(offset + fieldLen + 1U);
  }
  return false;
}

bool resultMatchesTarget(const BleActiveScanResult& result) {
  return advertisingDataHasName(result.advData(), result.advDataLength()) ||
         advertisingDataHasName(result.scanRspData(),
                                result.scanRspDataLength());
}

void printStatus() {
  int8_t rssiDbm = 0;
  (void)g_ble.getLatestConnectionRssiDbm(&rssiDbm);
  Serial.print("CENTRAL status secure=");
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
  Serial.print(" pass=");
  Serial.print(g_passCount);
  Serial.print(" fail=");
  Serial.print(g_failCount);
  Serial.print(" cycle=");
  Serial.println(g_cycleCount);
}

void printResults() {
  Serial.print("CENTRAL results pass=");
  Serial.print(g_passCount);
  Serial.print(" fail=");
  Serial.print(g_failCount);
  Serial.print(" cycle=");
  Serial.println(g_cycleCount);
}

void printFailureDebug() {
  BleSecureConnectionsDebugState sc{};
  g_ble.getSecureConnectionsDebugState(&sc);
  Serial.print("CENTRAL sc active=");
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
  Serial.print("CENTRAL enc main_enc_req=");
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
    Serial.print("CENTRAL disc reason=");
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

void failCurrent(const char* reason) {
  ++g_failCount;
  Serial.print("REGTEST FAIL test=");
  Serial.print(kTests[g_testIndex].name);
  Serial.print(" payload_index=");
  Serial.print(g_payloadIndex);
  Serial.print(" reason=");
  Serial.println(reason);
  printFailureDebug();
  g_phase = Phase::kDisconnect;
  g_phaseStartMs = millis();
}

void passPayload(uint8_t length) {
  ++g_passCount;
  Serial.print("REGTEST PASS payload test=");
  Serial.print(kTests[g_testIndex].name);
  Serial.print(" secure=");
  Serial.print(kSecure ? 1 : 0);
  Serial.print(" len=");
  Serial.print(length);
  Serial.print(" mtu=");
  Serial.print(g_ble.currentAttMtu());
  Serial.print(" dle=");
  Serial.print(g_ble.currentDataLength());
  Serial.print(" phy=");
  Serial.println(g_ble.currentTxPhy());
}

void handleCommand(const char* cmd) {
  if (strcmp(cmd, "status") == 0) {
    printStatus();
  } else if (strcmp(cmd, "results") == 0) {
    printResults();
  } else if (strcmp(cmd, "clear") == 0) {
    g_ble.clearBondRecord(true);
    Serial.println("CENTRAL bond-cleared");
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

void resetTestTransientState() {
  g_mtuRequested = false;
  g_dleRequested = false;
  g_phyRequested = false;
  g_subscribeQueued = false;
  g_expectNotification = false;
  g_writeQueued = false;
  g_payloadIndex = 0U;
  g_expectedLength = 0U;
}

void startCurrentTest() {
  if (g_testIndex >= (sizeof(kTests) / sizeof(kTests[0]))) {
    g_testIndex = 0U;
    ++g_cycleCount;
    Serial.print("REGTEST PASS cycle=");
    Serial.print(g_cycleCount);
    Serial.print(" secure=");
    Serial.print(kSecure ? 1 : 0);
    Serial.print(" pass=");
    Serial.print(g_passCount);
    Serial.print(" fail=");
    Serial.println(g_failCount);
  }

  resetTestTransientState();
  Serial.print("CENTRAL test-start ");
  Serial.print(kTests[g_testIndex].name);
  Serial.print(" secure=");
  Serial.println(kSecure ? 1 : 0);
  g_phase = Phase::kConnect;
  g_phaseStartMs = millis();
}

bool scanAndConnect() {
  BleActiveScanResult result{};
  if (!g_ble.scanActiveCycle(&result, kScanUs, kScanUs)) {
    return false;
  }
  if (!resultMatchesTarget(result)) {
    return false;
  }
  Serial.print("CENTRAL target rssi=");
  Serial.println(result.advRssiDbm);
  if (g_ble.initiateConnection(result.advertiserAddress,
                               result.advertiserAddressRandom,
                               kConnIntervalUnits, kConnTimeoutUnits, 9U,
                               kScanUs)) {
    Serial.println("CENTRAL connect-ok");
    return true;
  }
  Serial.println("CENTRAL connect-fail");
  return false;
}

void runNegotiation(uint32_t elapsedMs) {
  const TestCase& test = kTests[g_testIndex];
  if (!g_mtuRequested) {
    g_mtuRequested = true;
    g_ble.setCentralPreferredAttMtu(test.mtu);
    (void)g_ble.requestAttMtuExchange(test.mtu);
  }
  if (!g_dleRequested) {
    g_dleRequested = true;
    g_ble.setCentralPreferredDataLength(test.dle);
    (void)g_ble.requestDataLengthUpdate();
  }
  if (!g_phyRequested) {
    g_phyRequested = true;
    (void)g_ble.requestPHY(test.phy);
  }

  const bool mtuOk = (g_ble.currentAttMtu() == test.mtu);
  const bool dleOk = (g_ble.currentDataLength() == test.dle);
  const bool phyOk = (g_ble.currentTxPhy() == test.phy);
  if (mtuOk && dleOk && phyOk) {
    Serial.print("CENTRAL negotiated mtu=");
    Serial.print(g_ble.currentAttMtu());
    Serial.print(" dle=");
    Serial.print(g_ble.currentDataLength());
    Serial.print(" phy=");
    Serial.println(g_ble.currentTxPhy());
    g_phase = Phase::kSubscribe;
    g_phaseStartMs = millis();
    return;
  }

  if (elapsedMs > 12000UL) {
    failCurrent("negotiation-timeout");
  }
}

void runSubscribe(uint32_t elapsedMs) {
  const uint32_t queueTimeoutMs = kSecure ? 20000UL : 5000UL;
  if (!g_subscribeQueued) {
    g_subscribeQueued =
        g_ble.queueAttCccdWrite(kPeerNotifyCccdHandle, true, false, true);
    if (g_subscribeQueued) {
      Serial.println("CENTRAL subscribe-queued");
      return;
    }
    if (elapsedMs > queueTimeoutMs) {
      failCurrent("subscribe-queue-timeout");
    }
    return;
  }

  if (elapsedMs > 700UL) {
    g_phase = Phase::kPayload;
    g_phaseStartMs = millis();
    return;
  }

  if (elapsedMs > 5000UL) {
    failCurrent("subscribe-timeout");
  }
}

bool notificationMatches(const BleConnectionEvent& event) {
  if (event.payload == nullptr || event.payloadLength < 7U) {
    return false;
  }
  if (event.payload[4] != 0x1BU) {
    return false;
  }
  if (readLe16Local(&event.payload[5]) != kPeerNotifyHandle) {
    return false;
  }
  const uint8_t valueLength = static_cast<uint8_t>(event.payloadLength - 7U);
  return valueLength == g_expectedLength &&
         memcmp(&event.payload[7], g_expectedPayload, valueLength) == 0;
}

void handleConnectionEvent(const BleConnectionEvent& event) {
  if (kVerbose && g_expectNotification) {
    if (event.txPacketSent) {
      Serial.print("CENTRAL tx llid=");
      Serial.print(event.txLlid);
      Serial.print(" len=");
      Serial.print(event.txPayloadLength);
      Serial.print(" fresh=");
      Serial.print(event.freshTxAllowed ? 1 : 0);
      Serial.print(" ack=");
      Serial.println(event.peerAckedLastTx ? 1 : 0);
    }
    if (event.packetReceived && event.crcOk) {
      Serial.print("CENTRAL rx llid=");
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

  if (!g_expectNotification || !event.packetReceived || !event.crcOk ||
      !event.packetIsNew || !event.attPacket) {
    return;
  }
  if (!notificationMatches(event)) {
    if (event.payload != nullptr && event.payloadLength >= 5U &&
        event.payload[4] == 0x1BU) {
      failCurrent("notify-mismatch");
    }
    return;
  }

  passPayload(g_expectedLength);
  ++g_sequence;
  ++g_payloadIndex;
  g_expectNotification = false;
  g_writeQueued = false;
  g_phaseStartMs = millis();
}

void runPayload(uint32_t elapsedMs) {
  const uint32_t queueTimeoutMs = kSecure ? 20000UL : 7000UL;
  const uint32_t payloadTimeoutMs = kSecure ? 20000UL : 7000UL;
  const uint8_t length = payloadLengthForIndex(g_payloadIndex);
  if (length == 0U) {
    ++g_testIndex;
    g_phase = Phase::kDisconnect;
    g_phaseStartMs = millis();
    return;
  }

  if (!g_writeQueued) {
    buildExpectedPayload(length);
    if (g_ble.queueAttWriteRequest(kPeerWriteHandle, g_expectedPayload,
                                   g_expectedLength, true)) {
      g_writeQueued = true;
      g_expectNotification = true;
      Serial.print("CENTRAL write-queued seq=");
      Serial.print(g_sequence);
      Serial.print(" len=");
      Serial.println(g_expectedLength);
      return;
    }
    if (elapsedMs > queueTimeoutMs) {
      failCurrent("write-queue-timeout");
    }
    return;
  }

  if (elapsedMs > payloadTimeoutMs) {
    failCurrent("payload-timeout");
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
  Serial.println("CENTRAL start BleLinkPayloadRegtestCentral");
  Serial.print("CENTRAL secure=");
  Serial.println(kSecure ? 1 : 0);

  g_prefs.begin(kPrefsNs, false);
  g_ble.begin(kTxPowerDbm);
  g_ble.loadAddressFromFicr(true);
  g_ble.setBondPersistenceCallbacks(loadBond, saveBond, clearBond, nullptr);
  if (!kSecure) {
    g_ble.clearBondRecord(true);
  }
  g_ble.setPreferredPhyOptions(kBlePhy1M | kBlePhy2M, kBlePhy1M | kBlePhy2M);
  g_ble.setBackgroundConnectionServiceEnabled(true);
  BoardControl::setAntennaPath(BoardAntennaPath::kCeramic);

  startCurrentTest();
}

void loop() {
  delay(1);
  pollCommands();

  const bool connected = g_ble.isConnected();
  const uint32_t nowMs = millis();
  const uint32_t elapsedMs = nowMs - g_phaseStartMs;

  if (connected != g_prevConnected) {
    g_prevConnected = connected;
    Serial.println(connected ? "CENTRAL connected" : "CENTRAL disconnected");
  }

  if (connected) {
    BleConnectionEvent event{};
    if (g_ble.pollConnectionEvent(&event, kPollUs)) {
      handleConnectionEvent(event);
    }
  }

  switch (g_phase) {
    case Phase::kConnect:
      if (connected) {
        g_phase = kSecure ? Phase::kWaitSecurity : Phase::kNegotiate;
        g_phaseStartMs = nowMs;
      } else {
        (void)scanAndConnect();
      }
      if (elapsedMs > kPhaseTimeoutMs) {
        failCurrent("connect-timeout");
      }
      break;

	    case Phase::kWaitSecurity:
	      if (!connected) {
	        failCurrent("security-disconnect");
	      } else if (g_ble.isConnectionEncrypted() && g_ble.hasBondRecord()) {
	        Serial.println("CENTRAL encrypted");
	        g_phase = Phase::kNegotiate;
	        g_phaseStartMs = nowMs;
      } else if (elapsedMs > kPhaseTimeoutMs) {
        failCurrent("security-timeout");
      }
      break;

    case Phase::kNegotiate:
      if (!connected || (kSecure && !g_ble.isConnectionEncrypted())) {
        failCurrent("negotiation-link-lost");
      } else {
        runNegotiation(elapsedMs);
      }
      break;

    case Phase::kSubscribe:
      if (!connected || (kSecure && !g_ble.isConnectionEncrypted())) {
        failCurrent("subscribe-link-lost");
      } else {
        runSubscribe(elapsedMs);
      }
      break;

    case Phase::kPayload:
      if (!connected || (kSecure && !g_ble.isConnectionEncrypted())) {
        failCurrent("payload-link-lost");
      } else {
        runPayload(elapsedMs);
      }
      break;

    case Phase::kDisconnect:
      if (connected) {
        (void)g_ble.disconnect(300000UL);
      } else {
        g_phase = Phase::kWaitNext;
        g_phaseStartMs = nowMs;
      }
      break;

    case Phase::kWaitNext:
      if (elapsedMs > 1500UL) {
        startCurrentTest();
      }
      break;
  }

  static uint32_t lastStatusMs = 0UL;
  if ((nowMs - lastStatusMs) > 15000UL) {
    lastStatusMs = nowMs;
    printStatus();
  }
}
