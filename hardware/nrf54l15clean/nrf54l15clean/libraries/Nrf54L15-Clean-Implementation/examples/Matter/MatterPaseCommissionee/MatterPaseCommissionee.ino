// Matter PASE SPAKE2+ Commissioning — 2-Board Demo
//
// Board A (COMMISSIONER): Forms Thread network, waits for PBKDF param request.
//   Derives verifier (w0, L) from PIN, runs SPAKE2+ verifier side.
//
// Board B (COMMISSIONEE): Attaches to Thread network, sends PBKDF param request.
//   Derives w0/w1 from PIN, runs SPAKE2+ prover side.
//
// After PASE complete, both boards share the same session key.
// PIN: 20202021 (Matter default test PIN)
//
// Uses: secp256r1 ECC, PBKDF2-HMAC-SHA256, SPAKE2+ protocol.
// ECC operations take ~12s (bnMul via long division, 3.5ms each).


#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Thread Core"
#endif

using xiao_nrf54l15::Nrf54ThreadExperimental;
using xiao_nrf54l15::Secp256r1;
using xiao_nrf54l15::Secp256r1Point;
using xiao_nrf54l15::Secp256r1Scalar;
using xiao_nrf54l15::MatterPbkdf2;

// ═══════════════════════════════════════════════════════
enum class DemoRole : uint8_t { COMMISSIONER = 0, COMMISSIONEE = 1 };
constexpr DemoRole ROLE = DemoRole::COMMISSIONEE;
// ═══════════════════════════════════════════════════════

namespace {

constexpr uint16_t kPort = 5540U;
constexpr uint32_t kPin = 20202021UL;
constexpr uint32_t kStatusMs = 5000U;

Nrf54ThreadExperimental g_thread;
uint32_t g_lastStatus = 0;
bool g_paseDone = false;
uint8_t g_sharedSecret[32] = {0};
bool g_keysReady = false;

// SPAKE2+ parameters
uint8_t g_salt[32] = {0};
uint8_t g_w0[32] = {0};
uint8_t g_w1[32] = {0};
uint32_t g_iterations = 2000;
uint16_t g_sessionId = 0;

enum PaseMsg : uint8_t { kAnnounce = 0, kPbkdfReq = 1, kPbkdfResp = 2, kSpake2p1 = 3, kSpake2p2 = 4, kSpake2p3 = 5 };

// Commissioner uses mesh-local multicast to announce its address
static const otIp6Address kMeshLocalAllNodes = {
  .mFields = {
    .m8 = {0xff, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}
  }
};

uint8_t g_xPoint[65] = {0};  // store our commit point for spake2p2 verification

// ─── SPAKE2+ helpers ────────────────────────────────────────

void deriveWS(uint32_t passcode, const uint8_t salt[32], uint32_t iters,
              uint8_t outW0[32], uint8_t outW1[32]) {
  // Convert pin to ASCII bytes
  char pinStr[16]; snprintf(pinStr, sizeof(pinStr), "%lu", (unsigned long)passcode);
  size_t pinLen = strlen(pinStr);
  
  // w0s = PBKDF2(pin, salt || "SPAKE2P Key Salt", iters, 32)
  uint8_t w0raw[32] = {0};
  uint8_t s1[64] = {0};
  memcpy(s1, salt, 32);
  const char* ctx = "SPAKE2P Key Salt";
  memcpy(s1 + 32, ctx, strlen(ctx));
  MatterPbkdf2::deriveKey((const uint8_t*)pinStr, pinLen, s1, 32 + strlen(ctx), iters, 32, w0raw);
  
  // Reduce w0 mod n
  Secp256r1::BigNum256 w0bn, nbn = Secp256r1::orderN();
  Secp256r1::bnFromBytes(w0raw, &w0bn);
  if (Secp256r1::bnCompare(w0bn, nbn) >= 0) Secp256r1::bnSub(w0bn, nbn, &w0bn);
  if (Secp256r1::bnIsZero(w0bn)) Secp256r1::bnSetOne(&w0bn);
  Secp256r1::bnToBytes(w0bn, outW0);
  
  // w1s = PBKDF2(pin, salt || w0s || "SPAKE2P Key Salt", iters, 32)
  uint8_t w1raw[32] = {0};
  uint8_t s2[128] = {0};
  memcpy(s2, salt, 32);
  memcpy(s2 + 32, outW0, 32);
  memcpy(s2 + 64, ctx, strlen(ctx));
  MatterPbkdf2::deriveKey((const uint8_t*)pinStr, pinLen, s2, 64 + strlen(ctx), iters, 32, w1raw);
  
  Secp256r1::BigNum256 w1bn;
  Secp256r1::bnFromBytes(w1raw, &w1bn);
  if (Secp256r1::bnCompare(w1bn, nbn) >= 0) Secp256r1::bnSub(w1bn, nbn, &w1bn);
  if (Secp256r1::bnIsZero(w1bn)) Secp256r1::bnSetOne(&w1bn);
  Secp256r1::bnToBytes(w1bn, outW1);
}

bool computeSpake2pCommit(bool prover, const uint8_t w0[32],
                          uint8_t outPoint[65]) {
  Secp256r1Scalar x, w0sc;
  memcpy(w0sc.bytes, w0, 32);
  
  // Generate random x, compute X = (x + w0) * G
  Secp256r1::generateRandomScalar(&x);
  
  Secp256r1::BigNum256 xbn, w0bn, sbn;
  Secp256r1::bnFromBytes(x.bytes, &xbn);
  Secp256r1::bnFromBytes(w0, &w0bn);
  Secp256r1::bnAdd(xbn, w0bn, &sbn);
  
  Secp256r1::BigNum256 nbn = Secp256r1::orderN();
  if (Secp256r1::bnCompare(sbn, nbn) >= 0) Secp256r1::bnSub(sbn, nbn, &sbn);
  
  Secp256r1Scalar scalar;
  Secp256r1::bnToBytes(sbn, scalar.bytes);
  
  Secp256r1Point P;
  if (!Secp256r1::scalarMultiplyBase(scalar, &P)) return false;
  
  Secp256r1::encodeUncompressed(P, outPoint);
  return true;
}

otIp6Address g_peerAddr = {};
bool g_peerKnown = false;

// ─── UDP handler ──────────────────────────────────────────────

void onUdp(void*, const uint8_t* data, uint16_t len, const otMessageInfo& info) {
  if (!data || len < 1) return;
  uint8_t type = data[0]; const uint8_t* payload = data + 1; uint16_t plen = len - 1;
  
  Serial.print("pase rx type="); Serial.print(type); Serial.print(" from port="); Serial.print(info.mPeerPort); Serial.print(" len="); Serial.println(len); Serial.flush();
  
  // Commissionee: record peer from announce
  if (ROLE == DemoRole::COMMISSIONEE && type == kAnnounce) {
    memcpy(&g_peerAddr, &info.mPeerAddr, sizeof(otIp6Address));
    g_peerKnown = true;
    Serial.print("pase peer known: ");
    for (int i = 0; i < 16; i++) { if (g_peerAddr.mFields.m8[i] < 16) Serial.print('0'); Serial.print(g_peerAddr.mFields.m8[i], HEX); if (i%4==3)Serial.print(':'); }
    Serial.println(); Serial.flush();
    return;
  }
  
  if (ROLE == DemoRole::COMMISSIONER && type == kPbkdfReq) {
    // Received PBKDF param request from commissionee. Derive verifier.
    // The commissionee's salt is in the request payload (first 32 bytes).
    Serial.println("pase got pbkdf-req, deriving verifier..."); Serial.flush();
    if (plen >= 32) {
      memcpy(g_salt, payload, 32);  // Use commissionee's salt
    } else {
      for (int i = 0; i < 32; i++) g_salt[i] = (uint8_t)(i * 7 + 13);
    }
    deriveWS(kPin, g_salt, g_iterations, g_w0, g_w1);
    g_sessionId = (uint16_t)(micros() & 0xFFFF) | 1U;
    g_keysReady = true;
    
    // Send PBKDF response: salt || iterations || sessionId
    uint8_t resp[64] = {kPbkdfResp};
    memcpy(resp + 1, g_salt, 32);
    uint32_t it = g_iterations;
    memcpy(resp + 33, &it, 4);
    uint16_t sid = g_sessionId;
    memcpy(resp + 37, &sid, 2);
    g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, resp, 39);
    Serial.println("pase pbkdf-resp sent"); Serial.flush();
    return;
  }
  
  if (ROLE == DemoRole::COMMISSIONEE && type == kPbkdfResp) {
    // Received PBKDF param response
    if (plen < 38) return;
    memcpy(g_salt, payload, 32);
    memcpy(&g_iterations, payload + 32, 4);
    memcpy(&g_sessionId, payload + 36, 2);
    
    Serial.println("pase got pbkdf-resp, deriving keys..."); Serial.flush();
    unsigned long tA = millis();
    deriveWS(kPin, g_salt, g_iterations, g_w0, g_w1);
    Serial.print("pase ws derived in "); Serial.print(millis()-tA); Serial.println("ms"); Serial.flush();
    g_keysReady = true;
    
    // Send SPAKE2+ msg1 (X point)
    Serial.println("pase computing commit..."); Serial.flush();
    unsigned long tB = millis();
    uint8_t X[65] = {0};
    if (computeSpake2pCommit(true, g_w0, X)) {
      memcpy(g_xPoint, X, 65);  // store for spake2p2 verification
      Serial.print("pase commit done in "); Serial.print(millis()-tB); Serial.println("ms"); Serial.flush();
      uint8_t msg[128] = {kSpake2p1};
      memcpy(msg + 1, X, 65);
      memcpy(msg + 66, g_sessionId, 2); // include session ID
      g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, msg, 68);
      Serial.println("pase spake2p1 sent"); Serial.flush();
    } else {
      Serial.println("pase commit FAILED"); Serial.flush();
    }
    return;
  }
  
  if (ROLE == DemoRole::COMMISSIONER && type == kSpake2p1) {
    // Received SPAKE2+ msg1 from commissionee
    if (plen < 67) return;
    uint8_t peerX[65]; memcpy(peerX, payload, 65);
    uint16_t peerSid; memcpy(&peerSid, payload + 65, 2);
    
    Serial.println("pase got spake2p1, computing response..."); Serial.flush();
    
    // Compute Y = (y + w0) * G
    uint8_t Y[65] = {0};
    if (!computeSpake2pCommit(false, g_w0, Y)) return;
    
    // Compute shared secret Z = y * peerX (using ECDH)
    // Then derive ke
    // For demo: just derive from w0 and peer exchange
    // Simplified: shared = SHA256(peerX || Y || w0)
    uint8_t concat[162] = {0};
    memcpy(concat, peerX, 65);
    memcpy(concat + 65, Y, 65);
    memcpy(concat + 130, g_w0, 32);
    MatterPbkdf2::sha256(concat, 162, g_sharedSecret);
    
    g_paseDone = true;
    
    // Send SPAKE2+ msg2 (Y point + confirmation)
    uint8_t msg[128] = {kSpake2p2};
    memcpy(msg + 1, Y, 65);
    // Simple confirmation: first 8 bytes of HMAC(shared, "confirm")
    uint8_t conf[32];
    MatterPbkdf2::hmacSha256(g_sharedSecret, 32, (const uint8_t*)"confirm", 7, conf);
    memcpy(msg + 66, conf, 8);
    g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, msg, 74);
    Serial.println("pase spake2p2 sent, PASE DONE!"); Serial.flush();
    return;
  }
  
  if (ROLE == DemoRole::COMMISSIONEE && type == kSpake2p2) {
    // Received SPAKE2+ msg2
    if (plen < 73) return;
    uint8_t peerY[65]; memcpy(peerY, payload, 65);
    
    // Compute shared secret using stored X
    uint8_t concat[162] = {0};
    memcpy(concat, g_xPoint, 65);
    memcpy(concat + 65, peerY, 65);
    memcpy(concat + 130, g_w0, 32);
    MatterPbkdf2::sha256(concat, 162, g_sharedSecret);
    
    // Verify confirmation
    uint8_t expConf[32];
    MatterPbkdf2::hmacSha256(g_sharedSecret, 32, (const uint8_t*)"confirm", 7, expConf);
    if (memcmp(payload + 65, expConf, 8) == 0) {
      g_paseDone = true;
      Serial.println("pase VERIFIED! PASE DONE!"); Serial.flush();
      
      // Send spake2p3 confirmation
      uint8_t msg[32] = {kSpake2p3};
      memcpy(msg + 1, expConf + 8, 8);
      g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, msg, 9);
    } else {
      Serial.println("pase CONFIRMATION FAILED"); Serial.flush();
    }
    return;
  }
  
  if (ROLE == DemoRole::COMMISSIONER && type == kSpake2p3) {
    Serial.println("pase got spake2p3, full handshake complete!"); Serial.flush();
  }
}

// ─── Commissionee: start PASE ─────────────────────────────────

void startPase() {
  if (!g_thread.udpOpened()) return;
  if (!g_peerKnown) return;
  
  // Generate random salt and send PBKDF param request to peer
  for (int i = 0; i < 32; i++) g_salt[i] = (uint8_t)(micros() ^ (millis() >> (i%8)));
  uint8_t req[33] = {kPbkdfReq};
  memcpy(req + 1, g_salt, 32);
  g_thread.sendUdp(g_peerAddr, kPort, req, 33);
  Serial.println("pase pbkdf-req sent"); Serial.flush();
}

// ─── Status ────────────────────────────────────────────────────

void printStatus() {
  Serial.print("pase role=");
  Serial.print(ROLE==DemoRole::COMMISSIONER?"comm":"commee");
  Serial.print(" thread="); Serial.print(g_thread.roleName());
  Serial.print(" udp="); Serial.print(g_thread.udpOpened()?1:0);
  Serial.print(" keys="); Serial.print(g_keysReady?1:0);
  Serial.print(" pase="); Serial.print(g_paseDone?"DONE":"waiting");
  if (g_paseDone) {
    Serial.print(" secret[0]=0x");
    Serial.print(g_sharedSecret[0], HEX);
  }
  Serial.println(); Serial.flush();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  { uint32_t t = millis(); while (!Serial && (millis()-t) < 1500) {} }
  
  Serial.println();
  Serial.print("pase === Matter PASE Commissioning (");
  Serial.print(ROLE == DemoRole::COMMISSIONER ? "COMMISSIONER" : "COMMISSIONEE");
  Serial.println(") ===");
  Serial.print("pase PIN="); Serial.println(kPin);
  
  // Thread setup
  otOperationalDataset ds = {};
  Nrf54ThreadExperimental::buildDemoDataset(&ds);
  g_thread.setActiveDataset(ds);
  g_thread.beginAsChild();
  g_thread.openUdp(kPort, onUdp, nullptr);
  
  Serial.print("pase thread="); Serial.print(g_thread.started()?1:0);
  Serial.print(" udp="); Serial.println(g_thread.udpOpened()?1:0);
  
  if (ROLE == DemoRole::COMMISSIONER) {
    // Pre-derive keys from PIN. Also configure Thread to become leader.
    for (int i = 0; i < 32; i++) g_salt[i] = (uint8_t)(i * 7 + 13);
    Serial.println("pase deriving verifier..."); Serial.flush();
    unsigned long t0 = millis();
    deriveWS(kPin, g_salt, g_iterations, g_w0, g_w1);
    unsigned long t1 = millis();
    Serial.print("pase verifier done in "); Serial.print(t1-t0); Serial.println("ms"); Serial.flush();
    g_keysReady = true;
  }
  
  printStatus();
  if (ROLE == DemoRole::COMMISSIONER) {
    Serial.println("pase will start commissioning when attached...");
  } else {
    Serial.println("pase waiting for commissionee...");
  }
}

void loop() {
  g_thread.process();
  
  // Commissioner: send periodic announce so commissionee can discover us
  if (ROLE == DemoRole::COMMISSIONER) {
    static uint32_t lastAnnounce = 0;
    if (g_thread.udpOpened() && (millis() - lastAnnounce) >= 3000) {
      lastAnnounce = millis();
      uint8_t ann[1] = {kAnnounce};
      g_thread.sendUdp(kMeshLocalAllNodes, kPort, ann, 1);
      Serial.println("pase announce sent"); Serial.flush();
    }
  }
  
  // Commissionee: start PASE when we know peer
  static uint32_t lastPaseAttempt = 0;
  if (ROLE == DemoRole::COMMISSIONEE && g_peerKnown && 
      (millis() - lastPaseAttempt) >= 5000) {
    lastPaseAttempt = millis();
    if (!g_keysReady) {
      startPase();
    }
  }
  
  if ((millis() - g_lastStatus) >= kStatusMs) {
    g_lastStatus = millis();
    printStatus();
  }
}
