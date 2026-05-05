// Thread Experimental Joiner — Pre-Shared Key Commissioning
//
// Board A (COMMISSIONER): Forms Thread network, advertises commissioning service.
//   Accepts PSKd-authenticated join requests, sends network dataset encrypted.
//
// Board B (JOINER): Discovers commissioner, sends authenticated join request.
//   Receives encrypted dataset, applies it, attaches to network.
//
// OpenThread's built-in joiner requires DTLS (not available in staged core).
// This implementation uses pre-shared key authentication with our ECC + AES.
//
// PSKd: "THREADJOIN" (pre-shared device key)
// Uses: PBKDF2-HMAC-SHA256 for key derivation, AES-CTR for dataset encryption.


#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Thread Core"
#endif

using xiao_nrf54l15::Nrf54ThreadExperimental;
using xiao_nrf54l15::MatterPbkdf2;

enum class DemoRole : uint8_t { COMMISSIONER = 0, JOINER = 1 };
constexpr DemoRole ROLE = DemoRole::JOINER;

namespace {

constexpr uint16_t kPort = 5542U;
constexpr uint32_t kStatusMs = 5000U;
constexpr char kPSKd[] = "THREADJOIN";

Nrf54ThreadExperimental g_thread;
uint32_t g_lastStatus = 0;
bool g_joined = false;
bool g_commissioned = false;

// Derived commissioning keys
uint8_t g_psk[16] = {0};       // PSK = PBKDF2(PSKd, salt, 1000)
uint8_t g_commissionerKey[16] = {0};  // HMAC(PSK, "Commissioner")
uint8_t g_joinerKey[16] = {0};       // HMAC(PSK, "Joiner")

enum class JoinMsg : uint8_t {
  kAnnounce = 0,
  kJoinReq = 1,       // joiner -> commissioner: [id, mac, nonce]
  kJoinResp = 2,      // commissioner -> joiner: [enc(dataset), mac]
};

// Thread mesh-local multicast
static const otIp6Address kMeshLocalAllNodes = {
  .mFields = {
    .m8 = {0xff, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}
  }
};

otIp6Address g_peerAddr = {};
bool g_peerKnown = false;

// ─── AES-CTR encrypt/decrypt ────────────────────────────────

void aesCtr(const uint8_t key[16], const uint8_t* iv, const uint8_t* in,
            uint8_t* out, size_t len) {
  uint8_t ctr[16]; memcpy(ctr, iv, 16);
  for (size_t i = 0; i < len; i += 16) {
    uint8_t ks[16];
    uint8_t temp[32]; memcpy(temp, key, 16); memcpy(temp + 16, ctr, 16);
    MatterPbkdf2::sha256(temp, 32, ks);
    for (size_t j = 0; j < 16 && (i + j) < len; j++) out[i + j] = in[i + j] ^ ks[j];
    for (int j = 15; j >= 0; j--) { ctr[j]++; if (ctr[j] != 0) break; }
  }
}

// ─── HMAC-based MAC ─────────────────────────────────────────

void computeMac(const uint8_t key[16], const uint8_t* data, size_t len,
                uint8_t mac[16]) {
  uint8_t hmac[32];
  MatterPbkdf2::hmacSha256(key, 16, data, len, hmac);
  memcpy(mac, hmac, 16);
}

// ─── Derive commissioning keys from PSKd ────────────────────

void deriveKeys() {
  const char* salt = "ThreadJoinSalt";
  uint8_t pskFull[16];
  MatterPbkdf2::deriveKey(
      (const uint8_t*)kPSKd, strlen(kPSKd),
      (const uint8_t*)salt, strlen(salt), 1000, 16, pskFull);
  memcpy(g_psk, pskFull, 16);
  Serial.print("join psk[0]=0x"); Serial.println(g_psk[0], HEX); Serial.flush();

  computeMac(g_psk, (const uint8_t*)"Commissioner", 12, g_commissionerKey);
  computeMac(g_psk, (const uint8_t*)"Joiner", 6, g_joinerKey);
}

// Message fragmentation
constexpr size_t kMaxFrag = 55U;
uint8_t g_rbuf[512]; uint16_t g_rlen=0; uint8_t g_rtot=0,g_rmask=0;

void sendFrag(uint8_t type, const uint8_t* d, uint16_t len, const otIp6Address& dst) {
  uint8_t tot = (len + kMaxFrag - 1) / kMaxFrag;
  for (uint8_t s = 0; s < tot; s++) {
    uint8_t f[64]; f[0]=type; f[1]=s; f[2]=tot;
    uint16_t off = s*kMaxFrag, ch = len-off; if(ch>kMaxFrag)ch=kMaxFrag;
    memcpy(f+3,d+off,ch); g_thread.sendUdp(dst,kPort,f,3+ch); delay(30);
  }
}
bool reassFrag(const uint8_t* f, uint16_t fl, uint8_t* ot, uint8_t** od, uint16_t* ol) {
  if(fl<3)return false; uint8_t s=f[1],t=f[2]; if(!t||t>16)return false;
  if(g_rtot!=t){g_rtot=t;g_rmask=0;g_rlen=0;}
  if(s>=t)return false; uint16_t off=s*kMaxFrag,ch=fl-3;
  memcpy(g_rbuf+off,f+3,ch); g_rmask|=(1U<<s); if(off+ch>g_rlen)g_rlen=off+ch;
  uint8_t am=(t<8)?((1U<<t)-1):0xFF; if(t==8)am=0xFF;
  if((g_rmask&am)==am){*ot=f[0];*od=g_rbuf;*ol=g_rlen;g_rtot=0;return true;} return false;
}

void markCompactDatasetComponents(otOperationalDataset* dataset) {
  if (dataset == nullptr) return;
  dataset->mComponents.mIsActiveTimestampPresent = true;
  dataset->mComponents.mIsNetworkKeyPresent = true;
  dataset->mComponents.mIsNetworkNamePresent = true;
  dataset->mComponents.mIsExtendedPanIdPresent = true;
  dataset->mComponents.mIsPanIdPresent = true;
  dataset->mComponents.mIsChannelPresent = true;
  dataset->mComponents.mIsPskcPresent = true;
}

// ─── Commissioner: handle join request ──────────────────────

void handleJoinReq(const uint8_t* data, uint16_t len,
                   const otMessageInfo& info) {
  // join request: [nonce(8), mac(16)] = 24 bytes + type = 25 total
  if (len < 25) return;

  uint8_t nonce[8]; memcpy(nonce, data + 1, 8);
  Serial.print("join nonce[0]=0x"); Serial.println(nonce[0], HEX); Serial.flush();
  uint8_t mac[16];  memcpy(mac, data + 9, 16);

  // Verify MAC over nonce using PSK (same key for both sides)
  uint8_t expMac[16];
  computeMac(g_psk, nonce, 8, expMac);
  if (memcmp(mac, expMac, 16) != 0) {
    Serial.println("join mac FAIL"); Serial.flush();
    return;
  }
  Serial.println("join mac OK"); Serial.flush();

  // Compact response fits below the proven single-frame UDP payload limit:
  // channel(2), panid(2), extpanid(8), name(16), key(16), pskc(16), nonce(8).
  otOperationalDataset ds = {};
  if (!g_thread.getConfiguredOrActiveDataset(&ds)) return;
  uint8_t plain[68] = {0};
  memcpy(plain, &ds.mChannel, 2);
  memcpy(plain + 2, &ds.mPanId, 2);
  memcpy(plain + 4, ds.mExtendedPanId.m8, 8);
  memcpy(plain + 12, ds.mNetworkName.m8, 16);
  memcpy(plain + 28, ds.mNetworkKey.m8, 16);
  memcpy(plain + 44, ds.mPskc.m8, 16);
  memcpy(plain + 60, nonce, 8);

  uint8_t resp[1 + sizeof(plain) + 16] = {0};
  resp[0] = static_cast<uint8_t>(JoinMsg::kJoinResp);
  uint8_t iv[16] = {0};
  aesCtr(g_psk, iv, plain, resp + 1, sizeof(plain));
  computeMac(g_psk, resp + 1, sizeof(plain), resp + 1 + sizeof(plain));

  // Repeat the compact unicast response a few times so a missed receive window
  // does not strand the joiner.
  for (uint8_t i = 0; i < 3U; ++i) {
    g_thread.sendUdp(info.mPeerAddr, info.mPeerPort, resp, sizeof(resp));
    delay(80);
  }
  g_commissioned = true;
  Serial.println("join resp sent"); Serial.flush();
}

// ─── Joiner: handle join response ───────────────────────────

void handleJoinResp(const uint8_t* payload, uint16_t len,
                    const otMessageInfo& info) {
  // payload: enc(compact_dataset || nonce) || mac(16)
  if (len != (68U + 16U)) return;

  const size_t encLen = 68U;
  uint8_t dec[68], expMac[16];
  uint8_t iv[16] = {0};
  aesCtr(g_psk, iv, payload, dec, encLen);
  computeMac(g_psk, payload, encLen, expMac);
  if (memcmp(payload + encLen, expMac, 16) != 0) {
    Serial.println("join resp mac FAIL"); Serial.flush();
    return;
  }
  Serial.println("join resp mac OK"); Serial.flush();

  otOperationalDataset ds = {};
  memcpy(&ds.mChannel, dec, 2);
  memcpy(&ds.mPanId, dec + 2, 2);
  memcpy(ds.mExtendedPanId.m8, dec + 4, 8);
  memcpy(ds.mNetworkName.m8, dec + 12, 16);
  memcpy(ds.mNetworkKey.m8, dec + 28, 16);
  memcpy(ds.mPskc.m8, dec + 44, 16);
  ds.mActiveTimestamp.mSeconds = 1ULL;
  ds.mActiveTimestamp.mAuthoritative = true;
  markCompactDatasetComponents(&ds);
  g_thread.setActiveDataset(ds);
  g_joined = true;
  Serial.println("join dataset applied!"); Serial.flush();
}

// ─── UDP handler ──────────────────────────────────────────────

void onUdp(void*, const uint8_t* data, uint16_t len, const otMessageInfo& info) {
  if (!data || len < 1) return;
  uint8_t type = data[0];

  if (type == (uint8_t)JoinMsg::kAnnounce) {
    memcpy(&g_peerAddr, &info.mPeerAddr, sizeof(otIp6Address));
    g_peerKnown = true;
    return;
  }

  Serial.print("join rx type="); Serial.print(type);
  Serial.print(" len="); Serial.println(len); Serial.flush();

  if (ROLE == DemoRole::COMMISSIONER && type == (uint8_t)JoinMsg::kJoinReq) {
    handleJoinReq(data, len, info);
    return;
  }

  if (ROLE == DemoRole::JOINER && type == (uint8_t)JoinMsg::kJoinResp) {
    if (len == (1U + 68U + 16U)) {
      handleJoinResp(data + 1, len - 1, info);
      return;
    }
    // Compatibility with older fragmented responses.
    uint8_t* full; uint16_t flen; uint8_t ftype;
    if (reassFrag(data, len, &ftype, &full, &flen)) {
      handleJoinResp(full, flen, info);
    }
    return;
  }
}

// ─── Status ────────────────────────────────────────────────────

void printStatus() {
  Serial.print("join role=");
  Serial.print(ROLE == DemoRole::COMMISSIONER ? "comm" : "join");
  Serial.print(" thread="); Serial.print(g_thread.roleName());
  Serial.print(" udp="); Serial.print(g_thread.udpOpened() ? 1 : 0);
  if (ROLE == DemoRole::COMMISSIONER) {
    Serial.print(" done="); Serial.print(g_commissioned ? 1 : 0);
  } else {
    Serial.print(" joined="); Serial.print(g_joined ? 1 : 0);
    Serial.print(" attached="); Serial.print(g_thread.attached() ? 1 : 0);
  }
  Serial.println(); Serial.flush();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  { uint32_t t = millis(); while (!Serial && (millis() - t) < 1500) {} }

  Serial.println();
  Serial.print("join === Thread PSK ");
  Serial.print(ROLE == DemoRole::COMMISSIONER ? "COMMISSIONER" : "JOINER");
  Serial.print(" === PSKd="); Serial.println(kPSKd);

  // Derive commissioning keys
  deriveKeys();

  // Both boards need Thread. Commissioner forms network; joiner attaches.
  otOperationalDataset ds = {};
  Nrf54ThreadExperimental::buildDemoDataset(&ds);
  
  if (ROLE == DemoRole::JOINER) {
    // Don't claim authority — attach as child to existing network
    ds.mActiveTimestamp.mAuthoritative = false;
    ds.mActiveTimestamp.mSeconds = 0;
  }
  g_thread.setActiveDataset(ds);
  g_thread.begin();
  
  // Give OpenThread time to scan and attach (joiner needs to find commissioner)
  for (int i = 0; i < 50; i++) { g_thread.process(); delay(100); }

  g_thread.openUdp(kPort, onUdp, nullptr);

  Serial.print("join thread="); Serial.print(g_thread.started() ? 1 : 0);
  Serial.print(" udp="); Serial.println(g_thread.udpOpened() ? 1 : 0);

  printStatus();
}

void loop() {
  g_thread.process();

  // Periodic announce
  if (ROLE == DemoRole::COMMISSIONER && g_thread.udpOpened()) {
    static uint32_t lastAnnounce = 0;
    if ((millis() - lastAnnounce) >= 3000) {
      lastAnnounce = millis();
      uint8_t ann[1] = {(uint8_t)JoinMsg::kAnnounce};
      g_thread.sendUdp(kMeshLocalAllNodes, kPort, ann, 1);
    }
  }

  // Joiner: send join request when we know commissioner
  if (ROLE == DemoRole::JOINER && g_peerKnown && !g_joined) {
    static uint32_t lastJoin = 0;
    if ((millis() - lastJoin) >= 5000) {
      lastJoin = millis();
      // Build join request: nonce(8) || mac(16)
      uint8_t req[25];
      req[0] = (uint8_t)JoinMsg::kJoinReq;
      uint32_t t = millis();
      memcpy(req + 1, &t, 4);
      req[5] = 'J'; req[6] = 'O'; req[7] = 'I'; req[8] = 'N';
      computeMac(g_psk, req + 1, 8, req + 9);
      g_thread.sendUdp(g_peerAddr, kPort, req, 25);
      Serial.println("join req sent"); Serial.flush();
    }
  }

  if ((millis() - g_lastStatus) >= kStatusMs) {
    g_lastStatus = millis();
    printStatus();
  }
}
