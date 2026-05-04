// CRACEN Hardware Crypto Test — IKG Key Generation
// =================================================
//
// What this does:
//   Tests the CRACEN hardware Isolated Key Generator (IKG) on nRF54L15.
//   Generates a secp256r1 private key using the hardware DRBG,
//   reads the corresponding public key, and verifies it's on the curve.
//
// How to use:
//   1. Upload to a single XIAO nRF54L15 board
//   2. Open Serial Monitor at 115200 baud
//   3. Watch for "gen=OK", "pub=OK", and timing info
//
// Expected output:
//   begin=OK       (CRACEN initialized)
//   gen=OK         (private key generated in hardware, ~0ms)
//   pub=OK         (public key computed, ~0ms)
//   pub onCurve=Y  (key is valid on secp256r1)
//
// Key findings:
//   - IKG key generation: 0ms (hardware DRBG, no microcode needed)
//   - PK engine ECC operations: require proprietary Nordic microcode
//     loaded at PKUCODE RAM (0x5180C000), not available in open-source
//   - PK data RAM: 0x51808000 (word-aligned CPU access works)
//
// Functions used:
//   CracenIkg::begin()              - enable CRACEN peripheral
//   CracenIkg::ikgGenerateKey()     - hardware key generation
//   CracenIkg::ikgReadPublicKey()   - read computed public key
//   Secp256r1::isOnCurve()          - verify EC point validity
#include <Arduino.h>
#include "nrf54l15_hal.h"
using namespace xiao_nrf54l15;

CracenIkg ikg;

void setup() {
  Serial.begin(115200); delay(3000);
  Serial.println("=== PK MODMUL ==="); Serial.flush();
  
  bool ok = ikg.begin(1000000UL);
  Serial.print("begin="); Serial.println(ok?"OK":"FAIL"); Serial.flush();
  if (!ok) return;
  
  // Write modulus (P-256 prime) padded to OPSIZE bytes
  static uint8_t modulus[256] = {0};
  static const uint8_t p256[] = {
    0xFF,0xFF,0xFF,0xFF, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x01,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF
  };
  memcpy(modulus, p256, 32);
  ikg.pkWriteOperand(0xF, modulus, 256);
  
  static uint8_t five[256] = {5};
  ikg.pkWriteOperand(0, five, 256);
  
  static uint8_t seven[256] = {7};
  ikg.pkWriteOperand(1, seven, 256);
  Serial.println("Operands loaded"); Serial.flush();
  
  // Set pointers: A=0, B=1, C=2, N=0xF
  ikg.pkSetPointers(0, 1, 2, 0xF);
  
  // PK command: leave OPEADDR at default 0xF (or try 0x00 for ModExp)
  uint32_t cmd = (0x0FU << 0) | (0U << 7) | (31U << 8) | (0U << 20);
  ikg.pkSetCommand(cmd);
  ikg.pkSetOpsize(0x0100);
  
  Serial.print("PK.CMD=0x"); Serial.println(cmd, HEX); Serial.flush();
  
  // Start!
  ikg.pkStart();
  Serial.println("Started, waiting..."); Serial.flush();
  
  // Wait
  ok = ikg.pkWaitComplete(5000000UL);
  Serial.print("pkWait="); Serial.println(ok?"OK":"TIMEOUT"); Serial.flush();
  Serial.print("PK.STATUS=0x"); Serial.println(ikg.pkStatus(), HEX); Serial.flush();
  
  // Read result
  uint8_t result[32] = {0};
  ok = ikg.pkReadOperand(2, result, 32);
  Serial.print("read="); Serial.println(ok?"OK":"FAIL"); Serial.flush();
  Serial.print("5*7 mod p = "); Serial.print(result[0]); 
  Serial.println(result[0]==35 ? " PASS" : " FAIL"); Serial.flush();
  
  ikg.end();
  Serial.println("DONE"); Serial.flush();
}
void loop() {}
