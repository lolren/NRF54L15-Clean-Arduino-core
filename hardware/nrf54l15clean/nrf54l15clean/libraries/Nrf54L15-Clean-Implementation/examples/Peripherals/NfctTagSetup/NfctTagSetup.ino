/*
 * NFCT Tag Setup — NFC-A Target Header Configuration
 *
 * Reads the factory-programmed NFC tag header from FICR and demonstrates
 * configuring the NFCT NFCID1 registers used by automatic collision
 * resolution.
 *
 * On the XIAO nRF54L15, there is no NFC antenna, so this
 * example only demonstrates register-level setup. It can
 * be adapted for boards with NFC antenna routing.
 *
 * Hardware: XIAO nRF54L15
 * Serial:   115200 baud
 */

#include <Arduino.h>
#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println(F("======================================"));
  Serial.println(F("  NFCT — NFC-A Tag Setup"));
  Serial.println(F("======================================"));
  Serial.println();

  // ---- Read factory NFC ID from FICR ----
  Serial.println(F("--- Factory NFC ID (from FICR) ---"));
  uint8_t nfcId[16];
  Ficr::nfcId1(nfcId);

  Serial.print(F("  MFGID: 0x"));
  Serial.println(Ficr::nfcManufacturerId(), HEX);

  Serial.print(F("  NFCID1: "));
  for (int i = 0; i < 16; i++) {
    if (nfcId[i] < 0x10) Serial.print('0');
    Serial.print(nfcId[i], HEX);
    if (i < 15) Serial.print(':');
  }
  Serial.println();
  Serial.println();

  // ---- Current NFCT NFCID1 registers ----
  Serial.println(F("--- Current NFCT NFCID1 Registers ---"));
  Serial.print(F("  LAST:       0x"));
  Serial.println(Nfct::nfcIdLast(), HEX);
  Serial.print(F("  SECONDLAST: 0x"));
  Serial.println(Nfct::nfcIdSecondLast(), HEX);
  Serial.print(F("  THIRDLAST:  0x"));
  Serial.println(Nfct::nfcIdThirdLast(), HEX);
  Serial.println();

  // ---- Configure custom tag header ----
  Serial.println(F("--- Setting custom NFCID1 ---"));
  Serial.println(F("  Setting 10-byte NFCID1: 5F:DE:AD:BE:EF:CA:FE:12:34:56"));

  // NFCID1 bytes are supplied in normal on-air order. For 10-byte IDs the
  // wrapper writes THIRDLAST, SECONDLAST, and LAST as required by the NFCT
  // automatic collision-resolution engine.
  const uint8_t demoId[] = {0x5F, 0xDE, 0xAD, 0xBE, 0xEF,
                            0xCA, 0xFE, 0x12, 0x34, 0x56};
  if (!Nfct::setNfcId1(demoId, sizeof(demoId))) {
    Serial.println(F("  ERROR: NFCID1 rejected"));
  }

  Serial.println(F("  Tag headers updated."));
  Serial.println();

  // ---- Verify ----
  Serial.println(F("--- Verified NFCT NFCID1 ---"));
  uint8_t currentId[10];
  const uint8_t currentLen = Nfct::nfcId1(currentId, sizeof(currentId));
  Serial.print(F("  NFCID1 size: "));
  Serial.println(currentLen);
  Serial.print(F("  NFCID1: "));
  for (uint8_t i = 0; i < currentLen; ++i) {
    if (currentId[i] < 0x10) Serial.print('0');
    Serial.print(currentId[i], HEX);
    if ((i + 1U) < currentLen) Serial.print(':');
  }
  Serial.println();
  Serial.print(F("  LAST:       0x"));
  Serial.println(Nfct::nfcIdLast(), HEX);
  Serial.print(F("  SECONDLAST: 0x"));
  Serial.println(Nfct::nfcIdSecondLast(), HEX);
  Serial.print(F("  THIRDLAST:  0x"));
  Serial.println(Nfct::nfcIdThirdLast(), HEX);
  Serial.println();

  // ---- Configure NFCT features ----
  Serial.println(F("--- NFCT Feature Config ---"));
  Nfct::setLowPowerMode(true);
  Serial.println(F("  EasyDMA mode: full low-power"));
  Nfct::setNfcPadsEnabled(true);
  Serial.println(F("  NFC pads: enabled"));
  Nfct::setIoPolarity(true);  // Compatibility no-op on nRF54L15.
  Serial.println(F("  I/O polarity: nRF54L15 fixed/default"));
  Nfct::enableAutoResponse(true);
  Serial.println(F("  Auto collision resolution: enabled"));
  Nfct::setSensRes((2U << 6) | 0x04U);  // 10-byte NFCID1 plus SDD pattern.
  Serial.println(F("  SENS_RES: 10-byte NFCID1, SDD pattern 0x04"));
  Nfct::setSelRes(0x00);  // NFC Forum Type 2-like tag response.
  Serial.println(F("  SEL_RES: 0x00"));
  Serial.println();

  // ---- DMA buffer setup (for real use) ----
  Serial.println(F("--- DMA Buffer Setup (info only) ---"));
  static uint8_t packetBuffer[128];
  if (Nfct::setPacketBuffer(packetBuffer, sizeof(packetBuffer))) {
    Serial.println(F("  Shared EasyDMA packet buffer configured"));
  }
  Serial.println(F("  RX: enableRxData(), frameReceived(), rxByteCount()"));
  Serial.println(F("  TX: setTxAmount(), startTx(), frameTransmitted()"));
  Serial.print(F("  FIELDPRESENT now: "));
  Serial.println(Nfct::fieldPresent() ? F("yes") : F("no"));
  Serial.println();

  Serial.println(F("======================================"));
  Serial.println(F("  NFCT tag setup demo complete."));
  Serial.println(F("  (No antenna on XIAO — register config only)"));
  Serial.println(F("======================================"));

  while (true) delay(1000);
}

void loop() {
  // Not reached.
}
