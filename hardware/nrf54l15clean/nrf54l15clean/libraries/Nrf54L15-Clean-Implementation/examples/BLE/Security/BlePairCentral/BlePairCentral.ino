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
#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

// ═══════════════════════════════════════════════════════════
enum class DemoRole : uint8_t { PERIPHERAL = 0, CENTRAL = 1 };
constexpr DemoRole ROLE = DemoRole::CENTRAL;
// ═══════════════════════════════════════════════════════════

namespace {

constexpr char kAdvName[] = "X54-PAIR";
constexpr int8_t kTxPowerDbm = 0;
constexpr uint32_t kStatusMs = 3000U;
constexpr uint32_t kNotifyMs = 4000U;
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
bool g_prevConnected = false;
bool g_prevEncrypted = false;
bool g_centralSeen = false;

// ─── Bond persistence ─────────────────────────────────────────

static constexpr char kNs[] = "ble_pair";
static constexpr char kBondKey[] = "bond";

bool loadBond(void*, BleBondRecord* out) {
  if (g_prefs.getBytesLength(kBondKey) == sizeof(BleBondRecord)) {
    g_prefs.getBytes(kBondKey, out, sizeof(BleBondRecord));
    Serial.println("ble_pair bond-loaded");
    return true;
  }
  return false;
}

bool saveBond(void*, const BleBondRecord* rec) {
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

void onGattWrite(void*, uint16_t h, const uint8_t* d, uint8_t n) {
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

void handleCmd(const char* c) {
  if (strcmp(c,"status")==0) { printStatus(); return; }
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
  Serial.println("ble_pair cmd: status clear");
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
          g_ble.sendSmpSecurityRequest();
          Serial.println("ble_pair sent-smp-security-request");
        }
      } else {
        Serial.println("ble_pair disconnected");
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
      // Process connection events
      BleConnectionEvent evt;
      while (g_ble.pollConnectionEvent(&evt, 0U)) {}

      // Notify periodically when encrypted + subscribed
      if (encrypted && g_ble.isCustomGattCccdEnabled(g_notifyHandle) &&
          (millis() - g_lastNotify) >= kNotifyMs) {
        g_lastNotify = millis();
        g_notifySeq++;
        char buf[32];
        snprintf(buf, sizeof(buf), "ping-%lu", (unsigned long)g_notifySeq);
        g_ble.setCustomGattCharacteristicValue(g_notifyHandle,
                                               (const uint8_t*)buf, strlen(buf));
        g_ble.notifyCustomGattCharacteristic(g_notifyHandle);
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
        g_ble.setPreferredConnectionParameters(12U, 24U, 0U, 500U);
      } else {
        Serial.println("ble_pair central: disconnected");
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
                                       24U, 200U, 9U, 300000UL)) {
            Serial.println("OK");
          } else {
            Serial.println("FAIL");
          }
        }
      }
    } else {
      // Process events
      BleConnectionEvent evt;
      while (g_ble.pollConnectionEvent(&evt, 0U)) {}

      // Once encrypted, subscribe to notifications
      static bool subbed = false;
      if (encrypted && !subbed) {
        g_ble.queueAttCccdWrite(g_notifyCccd, true, false, true);
        subbed = true;
        Serial.println("ble_pair central: subscribed");
      }
      if (!encrypted) subbed = false;

      // Periodic write
      static uint32_t lastWr = 0;
      if (encrypted && (millis() - lastWr) >= 5000U) {
        lastWr = millis();
        static bool on = false; on = !on;
        const uint8_t v = on ? 1U : 0U;
        g_ble.queueAttWriteRequest(g_writeHandle, &v, 1U, false);
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
  if ((millis() - g_lastStatus) >= kStatusMs) {
    g_lastStatus = millis();
    printStatus();
  }
}
