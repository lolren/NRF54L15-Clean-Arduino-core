/*
 * BlePairingEncryptionStatus
 *
 * Observes BLE pairing and encryption state transitions in real time.
 * After connecting, pair from your phone (iOS or Android) and watch the
 * Serial Monitor for:
 *   "encryption=ON"   – SMP pairing + key distribution succeeded.
 *   "encryption=OFF"  – connection started unencrypted (normal before pairing).
 *
 * Diagnostic flags at the top of the file:
 *   kEnableBleTraceLogging   – print low-level HAL trace messages (verbose).
 *   kLogEveryConnectionEvent – print every connection event (very verbose).
 *
 * The printEncDebug() function dumps detailed encryption counters on disconnect,
 * useful for diagnosing MIC failures, key-exchange timing issues, etc.
 *
 * Note: kConstantLatency is used while BLE is active because some phones send
 * the SMP pairing PDU very early in the connection and the MCU must respond
 * quickly to avoid a timeout. Low-power latency modes can add too much jitter.
 */

#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;
static PowerManager g_power;
static bool g_bleReady = false;
// Set to true to enable verbose HAL-level trace messages on Serial.
static constexpr bool kEnableBleTraceLogging = true;
// Set to true to print one line per connection event (very noisy).
static constexpr bool kLogEveryConnectionEvent = false;
static constexpr bool kPrintEncryptionDebugOnDisconnect = false;
static constexpr bool kLogSmpPackets = true;
static constexpr bool kDumpTraceBufferOnDisconnect = true;
static constexpr uint8_t kTraceBufferDepth = 48U;
static constexpr uint8_t kTraceBufferEntryLen = 40U;

static bool g_prevConnected = false;
static bool g_prevEncrypted = false;
static bool g_connectionAnnounced = false;
static uint32_t g_lastAdvLogMs = 0U;
static uint32_t g_lastInitErrorLogMs = 0U;
static uint32_t g_setupMs = 0U;
static char g_traceBuffer[kTraceBufferDepth][kTraceBufferEntryLen] = {};
static uint8_t g_traceBufferHead = 0U;
static uint8_t g_traceBufferCount = 0U;
static constexpr int8_t kTxPowerDbm = 0;
static constexpr uint32_t kAdvertisingStartDelayMs = 4000UL;
static constexpr bool kRestrictToAllowedPeer = true;
static const uint8_t kAllowedPeerAddress[6] = {0xDF, 0x84, 0xE5,
                                               0x21, 0x34, 0x68};

static void printHexBytes(const uint8_t* data, size_t len) {
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

static void printEncDebug() {
  BleEncryptionDebugCounters dbg{};
  g_ble.getEncryptionDebugCounters(&dbg);
  Serial.print("enc_dbg followup_armed=");
  Serial.print(dbg.followupArmed);
  Serial.print(" end_seen=");
  Serial.print(dbg.followupEndSeen);
  Serial.print(" crc_ok=");
  Serial.print(dbg.followupCrcOk);
  Serial.print(" start_req=");
  Serial.print(dbg.followupStartEncReqSeen);
  Serial.print(" start_rsp_tx_ok=");
  Serial.print(dbg.followupStartEncRspTxOk);
  Serial.print(" rx_llid1=");
  Serial.print(dbg.followupRxLlid1);
  Serial.print(" rx_llid2=");
  Serial.print(dbg.followupRxLlid2);
  Serial.print(" rx_llid3=");
  Serial.print(dbg.followupRxLlid3);
  Serial.print(" last_hdr=0x");
  if (dbg.lastFollowHdr < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.lastFollowHdr, HEX);
  Serial.print(" last_llid=");
  Serial.print(dbg.lastFollowLlid);
  Serial.print(" last_len=");
  Serial.print(dbg.lastFollowLen);
  Serial.print(" last_b0=0x");
  if (dbg.lastFollowByte0 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.lastFollowByte0, HEX);
  Serial.print(" last_new=");
  Serial.print(dbg.lastFollowWasNew);
  Serial.print(" last_dec=");
  Serial.print(dbg.lastFollowWasDecrypted);
  Serial.print(" last_ctr_lo=");
  Serial.print(dbg.lastFollowCounterLo);

  Serial.print(" main_enc_req=");
  Serial.print(dbg.mainEncReqSeen);
  Serial.print(" main_enc_rsp_tx_ok=");
  Serial.print(dbg.mainEncRspTxOk);
  Serial.print(" main_start_req=");
  Serial.print(dbg.mainStartEncReqSeen);
  Serial.print(" main_start_req_dec=");
  Serial.print(dbg.mainStartEncReqSeenDecrypted);
  Serial.print(" main_start_rsp_tx_ok=");
  Serial.print(dbg.mainStartEncRspTxOk);

  Serial.print(" pend_ctrl_rx=");
  Serial.print(dbg.startPendingControlRxSeen);
  Serial.print(" pend_last_hdr=0x");
  if (dbg.startPendingLastHdr < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.startPendingLastHdr, HEX);
  Serial.print(" pend_last_len_raw=");
  Serial.print(dbg.startPendingLastLenRaw);
  Serial.print(" pend_last_b0=0x");
  if (dbg.startPendingLastByte0 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.startPendingLastByte0, HEX);
  Serial.print(" pend_last_dec=");
  Serial.print(dbg.startPendingLastDecrypted);
  Serial.print(" enc_rsp_txen_lag_last_us=");
  Serial.print(dbg.encRspTxenLagLastUs);
  Serial.print(" enc_rsp_txen_lag_max_us=");
  Serial.print(dbg.encRspTxenLagMaxUs);
  Serial.print(" mic_fail=");
  Serial.print(dbg.encRxMicFailCount);
  Serial.print(" short_pdu=");
  Serial.print(dbg.encRxShortPduCount);
  Serial.print(" mic_ctr_lo=");
  Serial.print(dbg.encRxLastMicFailCounterLo);
  Serial.print(" mic_hdr=0x");
  if (dbg.encRxLastMicFailHdr < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailHdr, HEX);
  Serial.print(" mic_len_raw=");
  Serial.print(dbg.encRxLastMicFailLenRaw);
  Serial.print(" mic_dir=");
  Serial.print(dbg.encRxLastMicFailDir);
  Serial.print(" mic_state=0x");
  if (dbg.encRxLastMicFailState < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailState, HEX);
  Serial.print(" mic_b=0x");
  if (dbg.encRxLastMicFailData0 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailData0, HEX);
  Serial.print(",");
  if (dbg.encRxLastMicFailData1 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailData1, HEX);
  Serial.print(",");
  if (dbg.encRxLastMicFailData2 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailData2, HEX);
  Serial.print(",");
  if (dbg.encRxLastMicFailData3 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailData3, HEX);
  Serial.print(",");
  if (dbg.encRxLastMicFailData4 < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encRxLastMicFailData4, HEX);
  Serial.print(" start_rsp_rx=");
  Serial.print(dbg.encStartRspRxCount);
  Serial.print(" start_rsp_raw_len=");
  Serial.print(dbg.encStartRspLastRawLen);
  Serial.print(" start_rsp_dec=");
  Serial.print(dbg.encStartRspLastDecrypted);
  Serial.print(" start_rsp_hdr=0x");
  if (dbg.encStartRspLastHdr < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encStartRspLastHdr, HEX);
  Serial.print(" pause_req=");
  Serial.print(dbg.encPauseReqAcceptedCount);
  Serial.print(" pause_rsp=");
  Serial.print(dbg.encPauseRspRxCount);
  Serial.print(" enc_clear=");
  Serial.print(dbg.encClearCount);
  Serial.print(" clear_reason=0x");
  if (dbg.encLastClearReason < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encLastClearReason, HEX);
  Serial.print(" tx_lag_last=");
  Serial.print(dbg.txenLagLastUs);
  Serial.print(" tx_lag_max=");
  Serial.print(dbg.txenLagMaxUs);
  Serial.print(" enc_tx_cnt=");
  Serial.print(dbg.encTxPacketCount);
  Serial.print(" enc_tx_lag_last=");
  Serial.print(dbg.encTxenLagLastUs);
  Serial.print(" enc_tx_lag_max=");
  Serial.print(dbg.encTxenLagMaxUs);
  Serial.print(" tx_ctr_lo=");
  Serial.print(dbg.encLastTxCounterLo);
  Serial.print(" tx_hdr=0x");
  if (dbg.encLastTxHdr < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encLastTxHdr, HEX);
  Serial.print(" tx_plen=");
  Serial.print(dbg.encLastTxPlainLen);
  Serial.print(" tx_alen=");
  Serial.print(dbg.encLastTxAirLen);
  Serial.print(" tx_fresh=");
  Serial.print(dbg.encLastTxWasFresh);
  Serial.print(" tx_enc=");
  Serial.print(dbg.encLastTxWasEncrypted);
  Serial.print(" rx_ctr_lo=");
  Serial.print(dbg.encLastRxCounterLo);
  Serial.print(" rx_hdr=0x");
  if (dbg.encLastRxHdr < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.encLastRxHdr, HEX);
  Serial.print(" rx_len_raw=");
  Serial.print(dbg.encLastRxLenRaw);
  Serial.print(" rx_new=");
  Serial.print(dbg.encLastRxWasNew);
  Serial.print(" rx_dec=");
  Serial.print(dbg.encLastRxWasDecrypted);
  Serial.print(" skdm=");
  printHexBytes(dbg.encLastSkdm, sizeof(dbg.encLastSkdm));
  Serial.print(" ivm=");
  printHexBytes(dbg.encLastIvm, sizeof(dbg.encLastIvm));
  Serial.print(" skds=");
  printHexBytes(dbg.encLastSkds, sizeof(dbg.encLastSkds));
  Serial.print(" ivs=");
  printHexBytes(dbg.encLastIvs, sizeof(dbg.encLastIvs));
  Serial.print(" stk=");
  printHexBytes(dbg.encLastStk, sizeof(dbg.encLastStk));
  Serial.print(" sk=");
  printHexBytes(dbg.encLastSessionKey, sizeof(dbg.encLastSessionKey));
  Serial.print(" sk_alt=");
  printHexBytes(dbg.encLastSessionAltKey, sizeof(dbg.encLastSessionAltKey));
  Serial.print(" sk_ok=");
  Serial.print(dbg.encLastSessionKeyValid);
  Serial.print(" sk_alt_ok=");
  Serial.print(dbg.encLastSessionAltKeyValid);
  Serial.print(" rx_dir=");
  Serial.print(dbg.encLastRxDir);
  Serial.print(" tx_dir=");
  Serial.print(dbg.encLastTxDir);
  Serial.print(" mic_raw=");
  printHexBytes(dbg.encRxLastMicFailPacket, dbg.encRxLastMicFailPacketLen);
  Serial.print("\r\n");
}

static void printScDebug() {
  BleSecureConnectionsDebugState dbg{};
  g_ble.getSecureConnectionsDebugState(&dbg);
  if (dbg.valid == 0U) {
    return;
  }

  Serial.print("sc_dbg active=");
  Serial.print(dbg.active);
  Serial.print(" local_init=");
  Serial.print(dbg.localInitiator);
  Serial.print(" wire_be=");
  Serial.print(dbg.wireFormatBigEndian);
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
  Serial.print(" conf_ready=");
  Serial.print(dbg.localConfirmReady);
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
  Serial.print(" lrand=");
  printHexBytes(dbg.localRandom, sizeof(dbg.localRandom));
  Serial.print(" prand=");
  printHexBytes(dbg.peerRandom, sizeof(dbg.peerRandom));
  Serial.print(" lconf=");
  printHexBytes(dbg.localConfirm, sizeof(dbg.localConfirm));
  Serial.print(" pconf=");
  printHexBytes(dbg.peerConfirm, sizeof(dbg.peerConfirm));
  Serial.print(" ldh=");
  printHexBytes(dbg.localDhKeyCheck, sizeof(dbg.localDhKeyCheck));
  Serial.print(" pdh=");
  printHexBytes(dbg.peerDhKeyCheck, sizeof(dbg.peerDhKeyCheck));
  Serial.print(" rdh=");
  printHexBytes(dbg.receivedDhKeyCheck, sizeof(dbg.receivedDhKeyCheck));
  Serial.print(" lpubx=");
  printHexBytes(dbg.localPublicKeyX, sizeof(dbg.localPublicKeyX));
  Serial.print(" ppubx=");
  printHexBytes(dbg.peerPublicKeyX, sizeof(dbg.peerPublicKeyX));
  Serial.print(" key_us=");
  Serial.print(dbg.localKeypairTimeUs);
  Serial.print(" dh_us=");
  Serial.print(dbg.dhKeyTimeUs);
  Serial.print(" chk_us=");
  Serial.print(dbg.checkValuesTimeUs);
  Serial.print(" hook=");
  Serial.print(dbg.cooperateHookCount);
  Serial.print(" bgsvc=");
  Serial.print(dbg.backgroundServiceCount);
  Serial.print("\r\n");
}

static void printDisconnectDebug() {
  BleDisconnectDebug dbg{};
  if (!g_ble.getDisconnectDebug(&dbg)) {
    return;
  }

  Serial.print("disc_dbg seq=");
  Serial.print(dbg.sequence);
  Serial.print(" reason=");
  Serial.print(dbg.reason);
  Serial.print(" err=0x");
  if (dbg.errorCode < 16U) {
    Serial.print('0');
  }
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
  Serial.print(" lasttx_llid=");
  Serial.print(dbg.lastTxLlid);
  Serial.print(" lasttx_len=");
  Serial.print(dbg.lastTxLength);
  Serial.print(" lasttx_op=0x");
  if (dbg.lastTxOpcode < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.lastTxOpcode, HEX);
  Serial.print(" lastrx_llid=");
  Serial.print(dbg.lastRxLlid);
  Serial.print(" lastrx_len=");
  Serial.print(dbg.lastRxLength);
  Serial.print(" lastrx_op=0x");
  if (dbg.lastRxOpcode < 16U) {
    Serial.print('0');
  }
  Serial.print(dbg.lastRxOpcode, HEX);
  Serial.print(" new=");
  Serial.print(dbg.lastPacketIsNew);
  Serial.print(" ack=");
  Serial.print(dbg.lastPeerAckedLastTx);
  Serial.print("\r\n");
}

static void appendTraceBuffer(const char* message) {
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

static void dumpTraceBuffer() {
  if (g_traceBufferCount == 0U) {
    return;
  }

  Serial.print("trace_dump count=");
  Serial.print(g_traceBufferCount);
  Serial.print("\r\n");
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
    Serial.print(g_traceBuffer[index]);
    Serial.print("\r\n");
  }
}

static void onBleTrace(const char* message, void* context) {
  (void)context;
  if (message == nullptr) {
    return;
  }
  appendTraceBuffer(message);
  Serial.print("[trace] ");
  Serial.print(message);
  Serial.print("\r\n");
}

static void printAddress(const uint8_t* addr) {
  if (addr == nullptr) {
    return;
  }

  for (int i = 5; i >= 0; --i) {
    if (addr[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(addr[i], HEX);
    if (i > 0) {
      Serial.print(':');
    }
  }
}

static bool addressesEqual(const uint8_t* lhs, const uint8_t* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return memcmp(lhs, rhs, 6U) == 0;
}

static uint16_t readLe16Local(const uint8_t* p) {
  if (p == nullptr) {
    return 0U;
  }
  return static_cast<uint16_t>(p[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8U);
}

static void logSmpEvent(const BleConnectionEvent& evt) {
  if (!evt.eventStarted) {
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
    if (code < 16U) {
      Serial.print('0');
    }
    Serial.print(code, HEX);
    const uint8_t dumpLen =
        (evt.payloadLength < 11U) ? evt.payloadLength : 11U;
    Serial.print(" bytes=");
    for (uint8_t i = 4U; i < dumpLen; ++i) {
      if (evt.payload[i] < 16U) {
        Serial.print('0');
      }
      Serial.print(evt.payload[i], HEX);
    }
    printed = true;
  }

  if ((evt.txLlid == 0x02U) && (evt.txPayloadLength >= 5U) &&
      (evt.txPayload != nullptr) &&
      (readLe16Local(&evt.txPayload[2]) == 0x0006U)) {
    if (printed) {
      Serial.print(" ");
    }
    Serial.print("smp_tx ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" code=0x");
    const uint8_t code = evt.txPayload[4];
    if (code < 16U) {
      Serial.print('0');
    }
    Serial.print(code, HEX);
    const uint8_t dumpLen =
        (evt.txPayloadLength < 11U) ? evt.txPayloadLength : 11U;
    Serial.print(" bytes=");
    for (uint8_t i = 4U; i < dumpLen; ++i) {
      if (evt.txPayload[i] < 16U) {
        Serial.print('0');
      }
      Serial.print(evt.txPayload[i], HEX);
    }
    printed = true;
  }

  if (printed) {
    Serial.print("\r\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(350);
  g_setupMs = millis();

  Serial.print("\r\nBlePairingEncryptionStatus start\r\n");

  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);
  if (kEnableBleTraceLogging) {
    g_ble.setTraceCallback(onBleTrace, nullptr);
  } else {
    g_ble.setTraceCallback(nullptr, nullptr);
  }

  static const uint8_t kAddress[6] = {0x51, 0x00, 0x15, 0x54, 0xDE, 0xC0};
  static constexpr bool kUseFixedAddress = true;
  bool ok = g_ble.begin(kTxPowerDbm);
  if (!ok) {
    Serial.print("BLE step failed: begin\r\n");
  }
  if (ok && (kUseFixedAddress && !g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic))) {
    ok = false;
    Serial.print("BLE step failed: setDeviceAddress\r\n");
  }
  if (ok && !g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd)) {
    ok = false;
    Serial.print("BLE step failed: setAdvertisingPduType\r\n");
  }
  if (ok && !g_ble.setAdvertisingName("X54-PAIR", true)) {
    ok = false;
    Serial.print("BLE step failed: setAdvertisingName\r\n");
  }
  if (ok && !g_ble.setScanResponseName("X54-PAIR-SCAN")) {
    ok = false;
    Serial.print("BLE step failed: setScanResponseName\r\n");
  }
  if (ok && !g_ble.setGattDeviceName("X54 Pairing Probe")) {
    ok = false;
    Serial.print("BLE step failed: setGattDeviceName\r\n");
  }
  if (ok && !g_ble.setGattBatteryLevel(96U)) {
    ok = false;
    Serial.print("BLE step failed: setGattBatteryLevel\r\n");
  }
  g_bleReady = ok;
  if (g_bleReady) {
    // kConstantLatency keeps CPU clocks ready during WFI so the SMP encryption
    // handshake can be completed without missing a connection anchor.
    g_power.setLatencyMode(PowerLatencyMode::kConstantLatency);
  }

  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
  char addressText[24] = {0};
  BleAddressType addressType = BleAddressType::kPublic;
  if (g_ble.getDeviceAddressString(addressText, sizeof(addressText),
                                   &addressType)) {
    Serial.print("BLE addr: ");
    Serial.print(addressText);
    Serial.print(" type=");
    Serial.print((addressType == BleAddressType::kRandomStatic) ? "random"
                                                                : "public");
    Serial.print("\r\n");
  }
  Serial.print("Pair from phone and watch encryption state transitions.\r\n");
}

void loop() {
  if (!g_bleReady) {
    const uint32_t now = millis();
    if ((now - g_lastInitErrorLogMs) >= 2000UL) {
      g_lastInitErrorLogMs = now;
      Serial.print("BLE not ready; skipping advertise/poll\r\n");
    }
    delay(10);
    return;
  }

  if (!g_ble.isConnected()) {
    if ((millis() - g_setupMs) < kAdvertisingStartDelayMs) {
      delay(1);
      return;
    }
    if (g_prevConnected) {
      g_prevConnected = false;
      g_prevEncrypted = false;
      g_connectionAnnounced = false;
      Serial.print("disconnected\r\n");
      if (kDumpTraceBufferOnDisconnect) {
        dumpTraceBuffer();
      }
      printDisconnectDebug();
      printScDebug();
      if (kPrintEncryptionDebugOnDisconnect) {
        printEncDebug();
      }
      Serial.flush();
      delay(20);
    }

    BleAdvInteraction adv{};
    g_ble.advertiseInteractEvent(&adv, 350U, 350000UL, 700000UL);
    if (adv.receivedConnectInd) {
      g_prevConnected = true;
      g_connectionAnnounced = false;
    } else {
      const uint32_t now = millis();
      if ((now - g_lastAdvLogMs) >= 2000UL) {
        g_lastAdvLogMs = now;
        Serial.print("advertising\r\n");
      }
    }

    Gpio::write(kPinUserLed, true);
    delay(1);
    return;
  }

  BleConnectionEvent evt{};
  const bool ran = g_ble.pollConnectionEvent(&evt, 450000UL);
  if (!g_connectionAnnounced && ran && evt.eventStarted) {
    BleConnectionInfo info{};
    if (g_ble.getConnectionInfo(&info)) {
      if (kRestrictToAllowedPeer &&
          !addressesEqual(info.peerAddress, kAllowedPeerAddress)) {
        Serial.print("reject peer=");
        printAddress(info.peerAddress);
        Serial.print("\r\n");
        g_ble.disconnect();
        delay(1);
        return;
      }
      Serial.print("connected peer=");
      printAddress(info.peerAddress);
      Serial.print(" int=");
      Serial.print(info.intervalUnits);
      Serial.print("\r\n");
    } else {
      Serial.print("connected\r\n");
    }
    g_ble.clearEncryptionDebugCounters();
    g_connectionAnnounced = true;
  }
  const bool encrypted = g_ble.isConnectionEncrypted();
  if (encrypted != g_prevEncrypted) {
    g_prevEncrypted = encrypted;
    Serial.print("encryption=");
    Serial.print(encrypted ? "ON" : "OFF");
    if (ran && evt.eventStarted) {
      Serial.print(" ce=");
      Serial.print(evt.eventCounter);
    }
    Serial.print("\r\n");
  }

  if (kEnableBleTraceLogging || kLogSmpPackets) {
    logSmpEvent(evt);
  }

  if (kLogEveryConnectionEvent && ran && evt.eventStarted) {
    Serial.print("ce=");
    Serial.print(evt.eventCounter);
    Serial.print(" rx=");
    Serial.print(evt.packetReceived ? 1 : 0);
    Serial.print(" crc=");
    Serial.print(evt.crcOk ? 1 : 0);
    Serial.print(" new=");
    Serial.print(evt.packetIsNew ? 1 : 0);
    Serial.print(" sn=");
    Serial.print(evt.rxSn);
    Serial.print(" nesn=");
    Serial.print(evt.rxNesn);
    Serial.print(" ack=");
    Serial.print(evt.peerAckedLastTx ? 1 : 0);
    Serial.print(" fresh=");
    Serial.print(evt.freshTxAllowed ? 1 : 0);
    Serial.print(" iack=");
    Serial.print(evt.implicitEmptyAck ? 1 : 0);
    Serial.print(" llid=");
    Serial.print(evt.llid);
    Serial.print(" len=");
    Serial.print(evt.payloadLength);
    Serial.print(" tx=");
    Serial.print(evt.txPacketSent ? 1 : 0);
    Serial.print(" txllid=");
    Serial.print(evt.txLlid);
    Serial.print(" txsn=");
    Serial.print(evt.txSn);
    Serial.print(" txnesn=");
    Serial.print(evt.txNesn);
    Serial.print(" txlen=");
    Serial.print(evt.txPayloadLength);
    if (evt.llControlPacket) {
      Serial.print(" rxop=0x");
      if (evt.llControlOpcode < 16U) {
        Serial.print('0');
      }
      Serial.print(evt.llControlOpcode, HEX);
    }
    if ((evt.txLlid == 0x03U) &&
        (evt.txPayloadLength >= 1U) &&
        (evt.txPayload != nullptr)) {
      Serial.print(" txop=0x");
      if (evt.txPayload[0] < 16U) {
        Serial.print('0');
      }
      Serial.print(evt.txPayload[0], HEX);
      const uint8_t txDump = (evt.txPayloadLength < 4U) ? evt.txPayloadLength : 4U;
      if (txDump > 1U) {
        Serial.print(" txb=");
        for (uint8_t i = 1U; i < txDump; ++i) {
          if (evt.txPayload[i] < 16U) {
            Serial.print('0');
          }
          Serial.print(evt.txPayload[i], HEX);
        }
      }
    }
    Serial.print(" term=");
    Serial.print(evt.terminateInd ? 1 : 0);
    Serial.print("\r\n");
  }

  if (ran && evt.terminateInd) {
    Serial.print("link terminated\r\n");
    if (kDumpTraceBufferOnDisconnect) {
      dumpTraceBuffer();
    }
    printDisconnectDebug();
    printScDebug();
    if (kPrintEncryptionDebugOnDisconnect) {
      printEncDebug();
    }
    Serial.flush();
    delay(20);
    g_prevConnected = false;
    g_prevEncrypted = false;
    g_connectionAnnounced = false;
  }

  Gpio::write(kPinUserLed, encrypted ? false : true);
  if (!ran) {
    delay(1);
  }
}
