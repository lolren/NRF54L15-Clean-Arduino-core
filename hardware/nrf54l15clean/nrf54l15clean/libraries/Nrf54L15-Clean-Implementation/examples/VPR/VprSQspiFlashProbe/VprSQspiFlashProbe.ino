// VPR sQSPI Flash Probe
// =======================
// Tests the VPR sQSPI SoftPeripheral: reads JEDEC ID, flash status,
// and optionally reads/writes flash memory via the VPR RISC-V CPU.
//
// Upload to a single XIAO nRF54L15 board (Sense variant has external flash).
// Open Serial Monitor at 115200 baud.
//
// Expected output:
//   VPR boot: OK
//   JEDEC ID: EF 40 18  (example — depends on flash chip)
//   Status: 0x00
//   Flash ready
//
// Note: The XIAO nRF54L15 Sense has a P25Q16H 2MB flash on the sQSPI pins.
// The non-Sense variant may not have external flash — JEDEC read will fail.
//
// Requires: VPR Support enabled in Tools menu (Tools > VPR Support > Enabled)

#include <nrf54_all.h>
#include "vpr_sqspi.h"

using xiao_nrf54l15::VprSQspi;

VprSQspi g_qspi;

void setup() {
  Serial.begin(115200);
  while (!Serial && (millis() - 1) < 3000) {}

  Serial.println();
  Serial.println("=== VPR sQSPI Flash Probe ===");
  Serial.println();

  // ─── Init ─────────────────────────────────────────────
  Serial.print("VPR sQSPI init: ");
  if (!g_qspi.begin()) {
    Serial.println("FAIL (check VPR Support enabled, XIAO Sense for flash)");
    return;
  }
  Serial.println("OK");

  // ─── JEDEC ID ─────────────────────────────────────────
  uint8_t jedec[3] = {0};
  if (g_qspi.getJedecId(jedec)) {
    Serial.print("JEDEC ID: ");
    for (int i = 0; i < 3; i++) {
      if (jedec[i] < 0x10) Serial.print('0');
      Serial.print(jedec[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  } else {
    Serial.println("JEDEC ID: FAIL (no flash chip?)");
    return;
  }

  // ─── Status ───────────────────────────────────────────
  uint8_t status = 0;
  if (g_qspi.readStatus(&status)) {
    Serial.print("Status: 0x");
    if (status < 0x10) Serial.print('0');
    Serial.println(status, HEX);
  }

  // ─── Unique ID ────────────────────────────────────────
  uint8_t uid[8] = {0};
  if (g_qspi.getUniqueId(uid)) {
    Serial.print("Unique ID: ");
    for (int i = 0; i < 8; i++) {
      if (uid[i] < 0x10) Serial.print('0');
      Serial.print(uid[i], HEX);
    }
    Serial.println();
  }

  // ─── Read test ────────────────────────────────────────
  uint8_t buf[64] = {0};
  if (g_qspi.read(0x000000, buf, 64)) {
    Serial.print("Read 0x000000: ");
    for (int i = 0; i < 16; i++) {
      if (buf[i] < 0x10) Serial.print('0');
      Serial.print(buf[i], HEX);
      Serial.print(' ');
    }
    Serial.println("...");
  }

  Serial.println();
  Serial.println("DONE");
}

void loop() {
  delay(1000);
}
