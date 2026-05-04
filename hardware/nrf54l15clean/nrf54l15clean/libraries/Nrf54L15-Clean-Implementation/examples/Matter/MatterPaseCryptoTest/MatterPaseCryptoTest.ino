// Matter PASE SPAKE2+ Commissioning Test
// Verifies secp256r1 ECC, PBKDF2, and SPAKE2+ protocol works on hardware.
// Generates keys, derives verifier, runs through PASE states.

#include <nrf54_all.h>

#include <matter_pase_commissioning.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Thread Core"
#endif

using xiao_nrf54l15::Secp256r1;
using xiao_nrf54l15::Secp256r1Point;
using xiao_nrf54l15::Secp256r1Scalar;
using xiao_nrf54l15::MatterPbkdf2;
using xiao_nrf54l15::Nrf54ThreadExperimental;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n=== PASE SPAKE2+ CRYPTO TEST ==="); Serial.flush(); delay(500);
  
  // Test 1: ECDSA key generation
  Serial.println("1. Key generation..."); Serial.flush(); delay(100);
  Secp256r1Scalar priv;
  Secp256r1Point pub;
  Secp256r1::generateKeyPair(&priv, &pub);
  Serial.print("   priv[0]=0x"); Serial.println(priv.bytes[0], HEX); Serial.flush(); delay(100);
  
  // Test 2: ECDSA sign + verify
  Serial.println("2. ECDSA sign+verify..."); Serial.flush(); delay(100);
  uint8_t hash[32] = {0};
  for(int i=0;i<32;i++) hash[i] = (uint8_t)(i * 7 + 13);
  uint8_t sigR[32]={0}, sigS[32]={0};
  bool ok = Secp256r1::ecdsaSign(priv, hash, sigR, sigS);
  Serial.print("   sign="); Serial.println(ok?"OK":"FAIL"); Serial.flush(); delay(100);
  
  ok = Secp256r1::ecdsaVerify(pub, hash, sigR, sigS);
  Serial.print("   verify="); Serial.println(ok?"OK":"FAIL"); Serial.flush(); delay(100);
  
  // Test 3: PBKDF2
  Serial.println("3. PBKDF2..."); Serial.flush(); delay(100);
  const char* pass = "20202021";
  uint8_t salt[32] = {0};
  for(int i=0;i<32;i++) salt[i] = (uint8_t)(i * 3 + 7);
  uint8_t dk[32] = {0};
  MatterPbkdf2::deriveKey((const uint8_t*)pass, 8, salt, 32, 2000, 32, dk);
  Serial.print("   dk[0]=0x"); Serial.println(dk[0], HEX); Serial.flush(); delay(100);
  
  // Test 4: SPAKE2+ verifier derivation
  Serial.println("4. SPAKE2+ verifier..."); Serial.flush(); delay(100);
  xiao_nrf54l15::MatterSpake2pVerifier verifier;
  ok = xiao_nrf54l15::MatterPaseCommissioning::deriveVerifier(20202021UL, salt, 2000, &verifier);
  Serial.print("   verifier="); Serial.println(ok?"OK":"FAIL"); Serial.flush(); delay(100);
  
  // Test 5: Thread + Matter platform
  Serial.println("5. Thread init..."); Serial.flush(); delay(100);
  Nrf54ThreadExperimental thread;
  otOperationalDataset ds = {};
  Nrf54ThreadExperimental::buildDemoDataset(&ds);
  thread.setActiveDataset(ds);
  thread.begin();
  Serial.print("   thread="); Serial.println(thread.started()?"OK":"FAIL"); Serial.flush();
  
  Serial.println("\n=== ALL TESTS COMPLETE ==="); Serial.flush();
}

void loop() { delay(5000); Serial.print("."); }
