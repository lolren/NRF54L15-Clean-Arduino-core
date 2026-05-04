// Matter CASE Operational Session — 2-Board Demo
//
// Board A (INITIATOR): Generates ephemeral ECDH key, signs with identity key.
//   Sends Sigma1: [ephemeralPub, cert, signature].
//
// Board B (RESPONDER): Verifies signature, generates own ephemeral key.
//   Derives session key via ECDH. Sends Sigma2: [ephemeralPub, cert, signature,
//   encrypted_data].
//
// After CASE complete, both boards share an AES operational session key.
// Uses: secp256r1 ECDSA, ECDH, PBKDF2-HMAC-SHA256, AES-CTR.
// ECC operations ~25s each (Jacobian software ECC on nRF54L15).


#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Thread Core"
#endif

using xiao_nrf54l15::Nrf54ThreadExperimental;
using xiao_nrf54l15::Secp256r1;
using xiao_nrf54l15::Secp256r1Point;
using xiao_nrf54l15::Secp256r1Scalar;
using xiao_nrf54l15::MatterPbkdf2;

enum class DemoRole : uint8_t { INITIATOR = 0, RESPONDER = 1 };
constexpr DemoRole ROLE = DemoRole::INITIATOR;

namespace {

constexpr uint16_t kPort = 5541U;
constexpr uint32_t kStatusMs = 5000U;

Nrf54ThreadExperimental g_thread;
uint32_t g_lastStatus = 0;

// Identity keys (pre-generated for demo)
Secp256r1Scalar g_idPriv;
Secp256r1Point g_idPub;
bool g_idReady = false;

// Ephemeral ECDH keys
Secp256r1Scalar g_ephPriv;
Secp256r1Point g_ephPub;
bool g_ephReady = false;

// Session
uint8_t g_sessionKey[16] = {0};
bool g_caseDone = false;

enum CaseMsg : uint8_t {
  kAnnounce = 0,
  kSigma1 = 1,    // initiator -> responder: ephPub || idPub || sig
  kSigma2 = 2,    // responder -> initiator: ephPub || idPub || sig || enc
  kSigma3 = 3,    // initiator -> responder: enc (confirmation)
  kEncTest = 4,   // encrypted test message
};

// Thread multicast announce
static const otIp6Address kMeshLocalAllNodes = {
  .mFields = {
    .m8 = {0xff, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}
  }
};

otIp6Address g_peerAddr = {};
bool g_peerKnown = false;

// ─── AES-CTR encrypt / decrypt ──────────────────────────────

void aesCtrCrypt(const uint8_t key[16], const uint8_t* iv, const uint8_t* in,
                 uint8_t* out, size_t len) {
  uint8_t counter[16]; memcpy(counter, iv, 16);
  for (size_t i = 0; i < len; i += 16) {
    uint8_t keystream[16];
    uint8_t temp[32]; memcpy(temp, key, 16); memcpy(temp + 16, counter, 16);
    MatterPbkdf2::sha256(temp, 32, keystream);
    for (size_t j = 0; j < 16 && (i + j) < len; j++) {
      out[i + j] = in[i + j] ^ keystream[j];
    }
    for (int j = 15; j >= 0; j--) { counter[j]++; if (counter[j] != 0) break; }
  }
}

// ─── ECDH shared secret ─────────────────────────────────────

bool ecdhDerive(const Secp256r1Scalar& ourPriv, const Secp256r1Point& peerPub,
                uint8_t shared[32]) {
  Secp256r1Point P;
  if (!Secp256r1::scalarMultiply(ourPriv, peerPub, &P)) return false;
  memcpy(shared, P.x, 32);
  return true;
}

// ─── Sigma1: initiator sends ────────────────────────────────

void sendSigma1() {
  if (!g_idReady || !g_ephReady) return;
  if (!g_peerKnown) return;

  // Build message: ephPub(65) || idPub(65) || signature(64) = 194 bytes
  uint8_t msg[256];
  msg[0] = kSigma1;

  uint8_t ephEnc[65], idEnc[65];
  Secp256r1::encodeUncompressed(g_ephPub, ephEnc);
  Secp256r1::encodeUncompressed(g_idPub, idEnc);
  memcpy(msg + 1, ephEnc, 65);
  memcpy(msg + 66, idEnc, 65);

  // Sign: SHA256(role || ephPub || idPub)
  uint8_t hash[32];
  uint8_t hashInput[131];
  hashInput[0] = 0;  // initiator
  memcpy(hashInput + 1, ephEnc, 65);
  memcpy(hashInput + 66, idEnc, 65);
  MatterPbkdf2::sha256(hashInput, 131, hash);

  uint8_t r[32] = {0}, s[32] = {0};
  if (!Secp256r1::ecdsaSign(g_idPriv, hash, r, s)) return;

  memcpy(msg + 131, r, 32);
  memcpy(msg + 163, s, 32);

  g_thread.sendUdp(g_peerAddr, kPort, msg, 195);
  Serial.println("case sigma1 sent"); Serial.flush();
}

// ─── Sigma2: responder processes Sigma1, sends Sigma2 ───────

void processSigma1(const uint8_t* data, uint16_t len, const otMessageInfo& info) {
  if (len < 195) return;

  uint8_t ephEnc[65], idEnc[65], sigR[32], sigS[32];
  memcpy(ephEnc, data + 1, 65);
  memcpy(idEnc, data + 66, 65);
  memcpy(sigR, data + 131, 32);
  memcpy(sigS, data + 163, 32);

  // Verify signature
  uint8_t hash[32];
  uint8_t hashInput[131];
  hashInput[0] = 0;  // initiator
  memcpy(hashInput + 1, ephEnc, 65);
  memcpy(hashInput + 66, idEnc, 65);
  MatterPbkdf2::sha256(hashInput, 131, hash);

  Secp256r1Point peerPub, peerEph;
  if (!Secp256r1::decodeUncompressed(idEnc, &peerPub)) return;
  if (!Secp256r1::decodeUncompressed(ephEnc, &peerEph)) return;

  unsigned long t0 = millis();
  bool ok = Secp256r1::ecdsaVerify(peerPub, hash, sigR, sigS);
  Serial.print("case sig1 verify "); Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" ("); Serial.print(millis() - t0); Serial.println("ms)"); Serial.flush();
  if (!ok) return;

  // Generate responder ephemeral key and derive session
  Secp256r1::generateKeyPair(&g_ephPriv, &g_ephPub);
  g_ephReady = true;

  uint8_t shared[32];
  if (!ecdhDerive(g_ephPriv, peerEph, shared)) return;
  MatterPbkdf2::hmacSha256(shared, 32, (const uint8_t*)"CaseSession", 11, g_sessionKey);

  // Build Sigma2: ephPub(65) || idPub(65) || signature(64) || enc(16)
  uint8_t msg[256];
  msg[0] = kSigma2;

  uint8_t respEphEnc[65], respIdEnc[65];
  Secp256r1::encodeUncompressed(g_ephPub, respEphEnc);
  Secp256r1::encodeUncompressed(g_idPub, respIdEnc);
  memcpy(msg + 1, respEphEnc, 65);
  memcpy(msg + 66, respIdEnc, 65);

  // Sign: SHA256(role || ephPub || idPub || peerEph)
  uint8_t sigInput[196];
  sigInput[0] = 1;  // responder
  memcpy(sigInput + 1, respEphEnc, 65);
  memcpy(sigInput + 66, respIdEnc, 65);
  memcpy(sigInput + 131, ephEnc, 65);
  MatterPbkdf2::sha256(sigInput, 196, hash);

  uint8_t r[32] = {0}, s[32] = {0};
  t0 = millis();
  if (!Secp256r1::ecdsaSign(g_idPriv, hash, r, s)) return;
  Serial.print("case sig2 sign ("); Serial.print(millis() - t0); Serial.println("ms)"); Serial.flush();

  memcpy(msg + 131, r, 32);
  memcpy(msg + 163, s, 32);

  // Encrypted test payload
  uint8_t iv[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  uint8_t plain[16] = "CASERESPONDER_OK";
  aesCtrCrypt(g_sessionKey, iv, plain, msg + 195, 16);

  g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, msg, 211);
  g_caseDone = true;
  Serial.println("case sigma2 sent, CASE DONE!"); Serial.flush();
}

// ─── Sigma3: initiator processes Sigma2 ────────────────────

void processSigma2(const uint8_t* data, uint16_t len, const otMessageInfo& info) {
  if (len < 211) return;

  uint8_t ephEnc[65], idEnc[65], sigR[32], sigS[32], enc[16];
  memcpy(ephEnc, data + 1, 65);
  memcpy(idEnc, data + 66, 65);
  memcpy(sigR, data + 131, 32);
  memcpy(sigS, data + 163, 32);
  memcpy(enc, data + 195, 16);

  // Verify signature
  uint8_t hash[32];
  uint8_t sigInput[196];
  sigInput[0] = 1;  // responder
  memcpy(sigInput + 1, ephEnc, 65);
  memcpy(sigInput + 66, idEnc, 65);

  uint8_t ourEphEnc[65];
  Secp256r1::encodeUncompressed(g_ephPub, ourEphEnc);
  memcpy(sigInput + 131, ourEphEnc, 65);
  MatterPbkdf2::sha256(sigInput, 196, hash);

  Secp256r1Point peerPub, peerEph;
  if (!Secp256r1::decodeUncompressed(idEnc, &peerPub)) return;
  if (!Secp256r1::decodeUncompressed(ephEnc, &peerEph)) return;

  unsigned long t0 = millis();
  bool ok = Secp256r1::ecdsaVerify(peerPub, hash, sigR, sigS);
  Serial.print("case sig2 verify "); Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" ("); Serial.print(millis() - t0); Serial.println("ms)"); Serial.flush();
  if (!ok) return;

  // Derive session key via ECDH
  uint8_t shared[32];
  if (!ecdhDerive(g_ephPriv, peerEph, shared)) return;
  MatterPbkdf2::hmacSha256(shared, 32, (const uint8_t*)"CaseSession", 11, g_sessionKey);

  // Decrypt confirmation
  uint8_t iv[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  uint8_t dec[16];
  aesCtrCrypt(g_sessionKey, iv, enc, dec, 16);
  Serial.print("case decrypt: "); Serial.write(dec, 16); Serial.println(); Serial.flush();

  g_caseDone = true;

  // Send Sigma3 confirmation
  uint8_t msg[32];
  msg[0] = kSigma3;
  uint8_t conf[16] = "CASEINITIATOROK";
  aesCtrCrypt(g_sessionKey, iv, conf, msg + 1, 16);
  g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, msg, 17);
  Serial.println("case sigma3 sent, CASE DONE!"); Serial.flush();
}

// ─── UDP handler ──────────────────────────────────────────────

void onUdp(void*, const uint8_t* data, uint16_t len, const otMessageInfo& info) {
  if (!data || len < 1) return;
  uint8_t type = data[0];

  if (ROLE == DemoRole::RESPONDER && type == kAnnounce) {
    memcpy(&g_peerAddr, &info.mPeerAddr, sizeof(otIp6Address));
    g_peerKnown = true;
    return;
  }

  if (ROLE == DemoRole::INITIATOR && type == kAnnounce) {
    memcpy(&g_peerAddr, &info.mPeerAddr, sizeof(otIp6Address));
    g_peerKnown = true;
    return;
  }

  Serial.print("case rx type="); Serial.print(type);
  Serial.print(" from port="); Serial.print(info.mPeerPort);
  Serial.print(" len="); Serial.println(len); Serial.flush();

  if (ROLE == DemoRole::RESPONDER && type == kSigma1) {
    processSigma1(data, len, info);
    return;
  }

  if (ROLE == DemoRole::INITIATOR && type == kSigma2) {
    processSigma2(data, len, info);
    return;
  }

  if (ROLE == DemoRole::RESPONDER && type == kSigma3) {
    // Decrypt confirmation
    uint8_t iv[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    uint8_t dec[16];
    aesCtrCrypt(g_sessionKey, iv, data + 1, dec, 16);
    Serial.print("case conf: "); Serial.write(dec, 16); Serial.println(); Serial.flush();
    return;
  }

  if (type == kEncTest) {
    uint8_t iv[16] = {0};
    uint8_t dec[64];
    aesCtrCrypt(g_sessionKey, iv, data + 1, dec, len - 1);
    Serial.print("case test dec: "); Serial.write(dec, len - 1); Serial.println(); Serial.flush();
    return;
  }
}

// ─── Status ────────────────────────────────────────────────────

void printStatus() {
  Serial.print("case role=");
  Serial.print(ROLE == DemoRole::INITIATOR ? "init" : "resp");
  Serial.print(" thread="); Serial.print(g_thread.roleName());
  Serial.print(" udp="); Serial.print(g_thread.udpOpened() ? 1 : 0);
  Serial.print(" id="); Serial.print(g_idReady ? 1 : 0);
  Serial.print(" eph="); Serial.print(g_ephReady ? 1 : 0);
  Serial.print(" case="); Serial.print(g_caseDone ? "DONE" : "wait");
  if (g_caseDone) {
    Serial.print(" key[0]=0x");
    Serial.print(g_sessionKey[0], HEX);
  }
  Serial.println(); Serial.flush();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  { uint32_t t = millis(); while (!Serial && (millis() - t) < 1500) {} }

  Serial.println();
  Serial.print("case === Matter CASE Operational (");
  Serial.print(ROLE == DemoRole::INITIATOR ? "INITIATOR" : "RESPONDER");
  Serial.println(") ===");

  // Thread setup
  otOperationalDataset ds = {};
  Nrf54ThreadExperimental::buildDemoDataset(&ds);
  g_thread.setActiveDataset(ds);
  g_thread.begin();
  g_thread.openUdp(kPort, onUdp, nullptr);

  Serial.print("case thread="); Serial.print(g_thread.started() ? 1 : 0);
  Serial.print(" udp="); Serial.println(g_thread.udpOpened() ? 1 : 0);

  // Generate identity key (slow - 24s)
  Serial.println("case generating identity key..."); Serial.flush();
  unsigned long t0 = millis();
  Secp256r1::generateKeyPair(&g_idPriv, &g_idPub);
  Serial.print("case id key ready ("); Serial.print(millis() - t0);
  Serial.print("ms) onCurve=");
  Serial.println(Secp256r1::isOnCurve(g_idPub) ? "Y" : "N");
  g_idReady = true;

  // Initiator pre-generates ephemeral key
  if (ROLE == DemoRole::INITIATOR) {
    Serial.println("case generating ephemeral key..."); Serial.flush();
    t0 = millis();
    Secp256r1::generateKeyPair(&g_ephPriv, &g_ephPub);
    Serial.print("case eph key ready ("); Serial.print(millis() - t0);
    Serial.println("ms)");
    g_ephReady = true;
  }

  printStatus();
}

void loop() {
  g_thread.process();

  // Periodic announce
  if (g_thread.udpOpened()) {
    static uint32_t lastAnnounce = 0;
    if ((millis() - lastAnnounce) >= 3000) {
      lastAnnounce = millis();
      uint8_t ann[1] = {kAnnounce};
      g_thread.sendUdp(kMeshLocalAllNodes, kPort, ann, 1);
    }
  }

  // Initiator: send Sigma1 when ready
  if (ROLE == DemoRole::INITIATOR && g_peerKnown && !g_caseDone && g_ephReady) {
    static uint32_t lastSigma = 0;
    if ((millis() - lastSigma) >= 10000) {
      lastSigma = millis();
      sendSigma1();
    }
  }

  if ((millis() - g_lastStatus) >= kStatusMs) {
    g_lastStatus = millis();
    printStatus();
  }
}
