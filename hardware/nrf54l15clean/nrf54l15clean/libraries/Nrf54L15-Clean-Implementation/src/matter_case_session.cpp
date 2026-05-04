#include "matter_case_session.h"

#include <Arduino.h>
#include <string.h>
#include "nrf54l15_hal.h"

namespace xiao_nrf54l15 {
namespace {

// HKDF-SHA256 key derivation
void hkdfSha256(const uint8_t* salt, size_t saltLen,
                const uint8_t* ikm, size_t ikmLen,
                const uint8_t* info, size_t infoLen,
                uint8_t* outKey, size_t outKeyLen) {
  // Extract: PRK = HMAC-SHA256(salt, IKM)
  uint8_t prk[32] = {0};
  MatterPbkdf2::hmacSha256(salt, saltLen, ikm, ikmLen, prk);

  // Expand: T(0) = empty, T(i) = HMAC(PRK, T(i-1) || info || i)
  uint8_t t[32] = {0};
  size_t generated = 0U;
  uint8_t counter = 1U;

  while (generated < outKeyLen) {
    uint8_t msg[64 + 32] = {0};
    size_t msgLen = 0U;
    if (counter > 1U) {
      memcpy(msg, t, 32);
      msgLen = 32U;
    }
    memcpy(msg + msgLen, info, infoLen);
    msgLen += infoLen;
    msg[msgLen++] = counter;

    MatterPbkdf2::hmacSha256(prk, 32, msg, msgLen, t);

    size_t copyLen = outKeyLen - generated;
    if (copyLen > 32U) copyLen = 32U;
    memcpy(outKey + generated, t, copyLen);
    generated += copyLen;
    counter++;
  }
}

void writeUint16Le(uint16_t v, uint8_t* b, size_t off) {
  b[off] = v & 0xFFU;
  b[off + 1] = (v >> 8U) & 0xFFU;
}

uint16_t readUint16Le(const uint8_t* b, size_t off) {
  return (uint16_t)b[off] | ((uint16_t)b[off + 1] << 8U);
}

void writeUint32Le(uint32_t v, uint8_t* b, size_t off) {
  b[off] = v & 0xFFU; b[off+1]=(v>>8)&0xFF; b[off+2]=(v>>16)&0xFF; b[off+3]=(v>>24)&0xFF;
}

// Software AES-CTR using hardware ECB
bool aesCtrCrypt(const uint8_t key[16],
                 const uint8_t* nonce, size_t nonceLen,
                 const uint8_t* input, size_t inputLen,
                 uint8_t* output) {
  Ecb ecb;
  // Build counter block from nonce
  uint8_t counter[16] = {0};
  size_t copyLen = nonceLen < 12U ? nonceLen : 12U;
  memcpy(counter, nonce, copyLen);

  size_t offset = 0U;
  uint32_t blockCounter = 0U;
  while (offset < inputLen) {
    // Set counter bytes in last 4 bytes
    counter[12] = (blockCounter >> 24U) & 0xFFU;
    counter[13] = (blockCounter >> 16U) & 0xFFU;
    counter[14] = (blockCounter >> 8U) & 0xFFU;
    counter[15] = blockCounter & 0xFFU;

    uint8_t keystream[16] = {0};
    if (!ecb.encryptBlock(key, counter, keystream)) {
      return false;
    }

    size_t chunk = inputLen - offset;
    if (chunk > 16U) chunk = 16U;
    for (size_t i = 0U; i < chunk; ++i) {
      output[offset + i] = input[offset + i] ^ keystream[i];
    }
    offset += chunk;
    blockCounter++;
  }
  return true;
}

// Simple HMAC-based AEAD for certificate encryption
bool aeadEncrypt(const uint8_t key[16],
                 const uint8_t* nonce, size_t nonceLen,
                 const uint8_t* plaintext, size_t plaintextLen,
                 uint8_t* outCiphertext, uint16_t* outLen) {
  // Encrypt with AES-CTR
  if (!aesCtrCrypt(key, nonce, nonceLen, plaintext, plaintextLen, outCiphertext)) {
    return false;
  }

  // Append HMAC-SHA256 tag (first 8 bytes as auth tag)
  uint8_t mac[32] = {0};
  uint8_t hmacInput[256] = {0};
  memcpy(hmacInput, nonce, nonceLen);
  memcpy(hmacInput + nonceLen, outCiphertext, plaintextLen);

  MatterPbkdf2::hmacSha256(key, 16, hmacInput, nonceLen + plaintextLen, mac);
  memcpy(outCiphertext + plaintextLen, mac, 8U);

  if (outLen != nullptr) *outLen = (uint16_t)(plaintextLen + 8U);
  return true;
}

bool aeadDecrypt(const uint8_t key[16],
                 const uint8_t* nonce, size_t nonceLen,
                 const uint8_t* ciphertext, size_t ciphertextLen,
                 uint8_t* outPlaintext, uint16_t* outLen) {
  if (ciphertextLen < 8U) return false;
  size_t ctLen = ciphertextLen - 8U;
  const uint8_t* tag = ciphertext + ctLen;

  // Verify HMAC
  uint8_t expectedMac[32] = {0};
  uint8_t hmacInput[256] = {0};
  memcpy(hmacInput, nonce, nonceLen);
  memcpy(hmacInput + nonceLen, ciphertext, ctLen);
  MatterPbkdf2::hmacSha256(key, 16, hmacInput, nonceLen + ctLen, expectedMac);

  if (memcmp(tag, expectedMac, 8U) != 0) return false;

  // Decrypt with AES-CTR
  if (!aesCtrCrypt(key, nonce, nonceLen, ciphertext, ctLen, outPlaintext)) {
    return false;
  }

  if (outLen != nullptr) *outLen = (uint16_t)ctLen;
  return true;
}

}  // namespace

// ─── Public API ──────────────────────────────────────────────────

bool MatterCaseSession::beginAsInitiator(StateCallback callback,
                                          void* context) {
  initiator_ = true;
  callback_ = callback;
  callbackContext_ = context;
  state_ = CaseState::kIdle;
  localSessionId_ = (uint16_t)(micros() & 0xFFFFU);
  if (localSessionId_ == 0U) localSessionId_ = 1U;
  return true;
}

bool MatterCaseSession::beginAsResponder(StateCallback callback,
                                          void* context) {
  initiator_ = false;
  callback_ = callback;
  callbackContext_ = context;
  state_ = CaseState::kIdle;
  localSessionId_ = (uint16_t)(micros() & 0xFFFFU) | 1U;
  return true;
}

void MatterCaseSession::end() {
  callback_ = nullptr;
  state_ = CaseState::kIdle;
  memset(&sessionKeys_, 0, sizeof(sessionKeys_));
}

bool MatterCaseSession::active() const {
  return state_ != CaseState::kIdle && state_ != CaseState::kFailed;
}

CaseState MatterCaseSession::state() const { return state_; }

const char* MatterCaseSession::stateName() const {
  return stateName(state_);
}

bool MatterCaseSession::setCertificate(const CaseCertificate& cert,
                                        const Secp256r1Scalar& privateKey) {
  localCert_ = cert;
  localPrivateKey_ = privateKey;
  // Derive public key from private
  Secp256r1::scalarMultiplyBase(privateKey, &localPublicKey_);
  return true;
}

bool MatterCaseSession::setPeerCertificate(const CaseCertificate& cert) {
  peerCert_ = cert;
  Secp256r1::decodeUncompressed(cert.subjectPubKey, &peerPublicKey_);
  return true;
}

bool MatterCaseSession::generateSelfSignedCert(
    const Secp256r1Scalar& privateKey,
    const Secp256r1Point& publicKey,
    uint16_t vendorId, uint16_t productId,
    CaseCertificate* outCert) {
  if (outCert == nullptr) return false;

  memset(outCert, 0, sizeof(*outCert));
  Secp256r1::encodeUncompressed(publicKey, outCert->subjectPubKey);

  // Issuer = self, so hash the same public key
  MatterPbkdf2::sha256(outCert->subjectPubKey, 65, outCert->issuerPubKeyHash);

  outCert->vendorId = vendorId;
  outCert->productId = productId;
  outCert->notBefore = (uint32_t)(millis() / 1000U);
  outCert->notAfter = outCert->notBefore + 86400U * 365U; // 1 year
  // Simple node ID = first 8 bytes of pub key hash
  memcpy(outCert->fabricId, outCert->issuerPubKeyHash, 8);
  memcpy(outCert->nodeId, outCert->issuerPubKeyHash + 8, 8);

  // Sign: SHA256(subjectPubKey || issuerPubKeyHash || vendorId || productId || fabricId || nodeId)
  uint8_t tbsData[128] = {0};
  size_t off = 0U;
  memcpy(tbsData + off, outCert->subjectPubKey, 65); off += 65;
  memcpy(tbsData + off, outCert->issuerPubKeyHash, 32); off += 32;
  writeUint16Le(vendorId, tbsData, off); off += 2;
  writeUint16Le(productId, tbsData, off); off += 2;
  memcpy(tbsData + off, outCert->fabricId, 8); off += 8;
  memcpy(tbsData + off, outCert->nodeId, 8); off += 8;

  uint8_t hash[32] = {0};
  MatterPbkdf2::sha256(tbsData, off, hash);

  if (!Secp256r1::ecdsaSign(privateKey, hash,
                            outCert->signature, outCert->signature + 32)) {
    return false;
  }

  outCert->valid = true;
  return true;
}

// ─── Sigma1 ──────────────────────────────────────────────────────

bool MatterCaseSession::buildSigma1(CaseSigma1* outMsg) {
  if (outMsg == nullptr) return false;

  memset(outMsg, 0, sizeof(*outMsg));
  generateRandom(outMsg->initiatorRandom, kCaseRandomSize);
  memcpy(initiatorRandom_, outMsg->initiatorRandom, kCaseRandomSize);

  outMsg->initiatorSessionId = localSessionId_;

  // Generate ephemeral key pair
  Secp256r1::generateKeyPair(&ephPrivateKey_, &ephPublicKey_);
  Secp256r1::encodeUncompressed(ephPublicKey_, outMsg->initiatorEphPubKey);

  // No resumption
  outMsg->resumptionIdLen = 0U;

  state_ = CaseState::kSigma1Sent;
  return true;
}

bool MatterCaseSession::processSigma1(const CaseSigma1& msg) {
  if (initiator_) return false;

  memcpy(initiatorRandom_, msg.initiatorRandom, kCaseRandomSize);
  peerSessionId_ = msg.initiatorSessionId;

  // Decode peer's ephemeral public key
  Secp256r1::decodeUncompressed(msg.initiatorEphPubKey, &peerEphPublicKey_);

  state_ = CaseState::kSigma1Received;
  return true;
}

// ─── Sigma2 ──────────────────────────────────────────────────────

bool MatterCaseSession::buildSigma2(CaseSigma2* outMsg) {
  if (outMsg == nullptr || initiator_) return false;

  memset(outMsg, 0, sizeof(*outMsg));
  generateRandom(outMsg->responderRandom, kCaseRandomSize);
  memcpy(responderRandom_, outMsg->responderRandom, kCaseRandomSize);

  outMsg->responderSessionId = localSessionId_;

  // Generate ephemeral key pair
  Secp256r1::generateKeyPair(&ephPrivateKey_, &ephPublicKey_);
  Secp256r1::encodeUncompressed(ephPublicKey_, outMsg->responderEphPubKey);

  // Derive shared secret via ECDH: shared = ephPrivate * peerEphPublic
  Secp256r1Point sharedPoint;
  if (!Secp256r1::scalarMultiply(ephPrivateKey_, peerEphPublicKey_, &sharedPoint)) {
    return false;
  }
  memcpy(sessionKeys_.sharedSecret, sharedPoint.x, 32);
  deriveSessionKeys();

  // Encrypt local certificate with i2rKey
  uint8_t certBuf[256] = {0};
  size_t certLen = 0U;
  // Serialize certificate: subjectPubKey(65) || issuerHash(32) || vendor(2) || product(2) || fabric(8) || node(8) || signature(64)
  memcpy(certBuf, localCert_.subjectPubKey, 65); certLen += 65;
  memcpy(certBuf + certLen, localCert_.issuerPubKeyHash, 32); certLen += 32;
  writeUint16Le(localCert_.vendorId, certBuf, certLen); certLen += 2;
  writeUint16Le(localCert_.productId, certBuf, certLen); certLen += 2;
  memcpy(certBuf + certLen, localCert_.fabricId, 8); certLen += 8;
  memcpy(certBuf + certLen, localCert_.nodeId, 8); certLen += 8;
  memcpy(certBuf + certLen, localCert_.signature, 64); certLen += 64;

  uint8_t nonce[13] = {0};
  memcpy(nonce, responderRandom_, 8);
  nonce[8] = 0x02;  // Sigma2 nonce marker

  if (!encryptWithKey(sessionKeys_.r2iKey, nonce, sizeof(nonce),
                      certBuf, certLen, nullptr, 0U,
                      outMsg->encryptedCert, &outMsg->encryptedCertLen)) {
    return false;
  }

  state_ = CaseState::kSigma2Sent;
  return true;
}

bool MatterCaseSession::processSigma2(const CaseSigma2& msg) {
  if (!initiator_) return false;

  memcpy(responderRandom_, msg.responderRandom, kCaseRandomSize);
  peerSessionId_ = msg.responderSessionId;

  // Decode peer's ephemeral public key
  Secp256r1::decodeUncompressed(msg.responderEphPubKey, &peerEphPublicKey_);

  // Derive shared secret via ECDH
  Secp256r1Point sharedPoint;
  if (!Secp256r1::scalarMultiply(ephPrivateKey_, peerEphPublicKey_, &sharedPoint)) {
    return false;
  }
  memcpy(sessionKeys_.sharedSecret, sharedPoint.x, 32);
  deriveSessionKeys();

  // Decrypt certificate
  uint8_t nonce[13] = {0};
  memcpy(nonce, responderRandom_, 8);
  nonce[8] = 0x02;

  uint8_t certBuf[256] = {0};
  uint16_t certLen = 0U;
  if (!decryptWithKey(sessionKeys_.r2iKey, nonce, sizeof(nonce),
                      msg.encryptedCert, msg.encryptedCertLen,
                      nullptr, 0U, certBuf, &certLen)) {
    advanceState(CaseState::kFailed);
    return false;
  }

  // Parse certificate
  if (certLen >= 181) {  // 65+32+2+2+8+8+64 = 181
    memcpy(peerCert_.subjectPubKey, certBuf, 65);
    memcpy(peerCert_.issuerPubKeyHash, certBuf + 65, 32);
    peerCert_.vendorId = readUint16Le(certBuf, 97);
    peerCert_.productId = readUint16Le(certBuf, 99);
    memcpy(peerCert_.fabricId, certBuf + 101, 8);
    memcpy(peerCert_.nodeId, certBuf + 109, 8);
    memcpy(peerCert_.signature, certBuf + 117, 64);
    peerCert_.valid = true;

    Secp256r1::decodeUncompressed(peerCert_.subjectPubKey, &peerPublicKey_);
  }

  state_ = CaseState::kSigma2Received;
  return true;
}

// ─── Sigma3 ──────────────────────────────────────────────────────

bool MatterCaseSession::buildSigma3(CaseSigma3* outMsg) {
  if (outMsg == nullptr || !initiator_) return false;

  memset(outMsg, 0, sizeof(*outMsg));

  // Serialize and encrypt local certificate
  uint8_t certBuf[256] = {0};
  size_t certLen = 0U;
  memcpy(certBuf, localCert_.subjectPubKey, 65); certLen += 65;
  memcpy(certBuf + certLen, localCert_.issuerPubKeyHash, 32); certLen += 32;
  writeUint16Le(localCert_.vendorId, certBuf, certLen); certLen += 2;
  writeUint16Le(localCert_.productId, certBuf, certLen); certLen += 2;
  memcpy(certBuf + certLen, localCert_.fabricId, 8); certLen += 8;
  memcpy(certBuf + certLen, localCert_.nodeId, 8); certLen += 8;
  memcpy(certBuf + certLen, localCert_.signature, 64); certLen += 64;

  uint8_t nonce[13] = {0};
  memcpy(nonce, initiatorRandom_, 8);
  nonce[8] = 0x03;

  if (!encryptWithKey(sessionKeys_.i2rKey, nonce, sizeof(nonce),
                      certBuf, certLen, nullptr, 0U,
                      outMsg->encryptedCert, &outMsg->encryptedCertLen)) {
    return false;
  }

  state_ = CaseState::kSigma3Sent;
  advanceState(CaseState::kEstablished);
  return true;
}

bool MatterCaseSession::processSigma3(const CaseSigma3& msg) {
  if (initiator_) return false;

  uint8_t nonce[13] = {0};
  memcpy(nonce, initiatorRandom_, 8);
  nonce[8] = 0x03;

  uint8_t certBuf[256] = {0};
  uint16_t certLen = 0U;
  if (!decryptWithKey(sessionKeys_.i2rKey, nonce, sizeof(nonce),
                      msg.encryptedCert, msg.encryptedCertLen,
                      nullptr, 0U, certBuf, &certLen)) {
    advanceState(CaseState::kFailed);
    return false;
  }

  if (certLen >= 181) {
    memcpy(peerCert_.subjectPubKey, certBuf, 65);
    memcpy(peerCert_.issuerPubKeyHash, certBuf + 65, 32);
    peerCert_.vendorId = readUint16Le(certBuf, 97);
    peerCert_.productId = readUint16Le(certBuf, 99);
    memcpy(peerCert_.fabricId, certBuf + 101, 8);
    memcpy(peerCert_.nodeId, certBuf + 109, 8);
    memcpy(peerCert_.signature, certBuf + 117, 64);
    peerCert_.valid = true;

    Secp256r1::decodeUncompressed(peerCert_.subjectPubKey, &peerPublicKey_);
  }

  state_ = CaseState::kEstablished;
  advanceState(CaseState::kEstablished);
  return true;
}

// ─── Session Keys ────────────────────────────────────────────────

bool MatterCaseSession::getSessionKeys(CaseSessionKeys* outKeys) const {
  if (outKeys == nullptr || !sessionKeys_.valid) return false;
  *outKeys = sessionKeys_;
  return true;
}

bool MatterCaseSession::deriveSessionKeys() {
  // HKDF-Expand from shared secret
  uint8_t salt[32] = {0};
  uint8_t infoBuf[64] = {0};
  size_t infoLen = 0U;

  // "Session Keys" info
  const char* info = "Session Keys";
  infoLen = strlen(info);
  memcpy(infoBuf, info, infoLen);

  uint8_t keyMaterial[64] = {0};
  hkdfSha256(salt, 32, sessionKeys_.sharedSecret, 32,
             (const uint8_t*)info, infoLen, keyMaterial, 64);

  // Split: first 16 bytes = i2r key, next 16 = r2i key
  if (initiator_) {
    memcpy(sessionKeys_.i2rKey, keyMaterial, 16);
    memcpy(sessionKeys_.r2iKey, keyMaterial + 16, 16);
  } else {
    memcpy(sessionKeys_.r2iKey, keyMaterial, 16);
    memcpy(sessionKeys_.i2rKey, keyMaterial + 16, 16);
  }

  sessionKeys_.valid = true;
  return true;
}

// ─── AES-CCM Encrypt/Decrypt ─────────────────────────────────────

bool MatterCaseSession::encryptMessage(
    const uint8_t* plaintext, uint16_t plaintextLen,
    const uint8_t* aad, uint16_t aadLen,
    uint8_t* outCiphertext, uint16_t* outLen,
    bool initiatorToResponder) {
  if (outLen != nullptr) *outLen = 0U;
  if (!sessionKeys_.valid) return false;

  const uint8_t* key = initiatorToResponder
                           ? sessionKeys_.i2rKey
                           : sessionKeys_.r2iKey;

  // Build nonce from session + counter
  static uint32_t encCounter = 0U;
  encCounter++;
  uint8_t nonce[13] = {0};
  memcpy(nonce, initiator_ ? initiatorRandom_ : responderRandom_, 8);
  writeUint32Le(encCounter, nonce, 8);
  nonce[12] = initiatorToResponder ? 0x01 : 0x02;

  (void)aad; (void)aadLen;
  return aeadEncrypt(key, nonce, 13, plaintext, plaintextLen,
                     outCiphertext, outLen);
}

bool MatterCaseSession::decryptMessage(
    const uint8_t* ciphertext, uint16_t ciphertextLen,
    const uint8_t* aad, uint16_t aadLen,
    uint8_t* outPlaintext, uint16_t* outLen,
    bool initiatorToResponder) {
  if (outLen != nullptr) *outLen = 0U;
  if (!sessionKeys_.valid || ciphertextLen < 8U) return false;

  const uint8_t* key = initiatorToResponder
                           ? sessionKeys_.i2rKey
                           : sessionKeys_.r2iKey;

  static uint32_t decCounter = 0U;
  decCounter++;
  uint8_t nonce[13] = {0};
  memcpy(nonce, initiator_ ? responderRandom_ : initiatorRandom_, 8);
  writeUint32Le(decCounter, nonce, 8);
  nonce[12] = initiatorToResponder ? 0x01 : 0x02;

  (void)aad; (void)aadLen;
  return aeadDecrypt(key, nonce, 13, ciphertext, ciphertextLen,
                     outPlaintext, outLen);
}

// ─── Certificate Verification ────────────────────────────────────

bool MatterCaseSession::verifyCertificate(const CaseCertificate& cert,
                                           const Secp256r1Point& issuerPubKey) {
  if (!cert.valid) return false;

  // Rebuild TBS data
  uint8_t tbsData[256] = {0};
  size_t off = 0U;
  memcpy(tbsData + off, cert.subjectPubKey, 65); off += 65;
  memcpy(tbsData + off, cert.issuerPubKeyHash, 32); off += 32;
  writeUint16Le(cert.vendorId, tbsData, off); off += 2;
  writeUint16Le(cert.productId, tbsData, off); off += 2;
  memcpy(tbsData + off, cert.fabricId, 8); off += 8;
  memcpy(tbsData + off, cert.nodeId, 8); off += 8;

  uint8_t hash[32] = {0};
  MatterPbkdf2::sha256(tbsData, off, hash);

  return Secp256r1::ecdsaVerify(issuerPubKey, hash,
                                cert.signature, cert.signature + 32);
}

// ─── Internal Helpers ───────────────────────────────────────────

bool MatterCaseSession::encryptWithKey(
    const uint8_t key[16], const uint8_t* nonce, size_t nonceLen,
    const uint8_t* plaintext, size_t plaintextLen,
    const uint8_t* aad, size_t aadLen,
    uint8_t* outCiphertext, uint16_t* outLen) {
  (void)aad; (void)aadLen;
  return aeadEncrypt(key, nonce, nonceLen, plaintext, plaintextLen,
                     outCiphertext, outLen);
}

bool MatterCaseSession::decryptWithKey(
    const uint8_t key[16], const uint8_t* nonce, size_t nonceLen,
    const uint8_t* ciphertext, size_t ciphertextLen,
    const uint8_t* aad, size_t aadLen,
    uint8_t* outPlaintext, uint16_t* outLen) {
  (void)aad; (void)aadLen;
  return aeadDecrypt(key, nonce, nonceLen, ciphertext, ciphertextLen,
                     outPlaintext, outLen);
}

void MatterCaseSession::generateRandom(uint8_t* out, size_t len) {
  if (out == nullptr) return;
  for (size_t i = 0; i < len; ++i) {
    out[i] = (uint8_t)(micros() ^ (millis() >> (i % 8)));
  }
}

void MatterCaseSession::advanceState(CaseState newState) {
  state_ = newState;
  if (callback_ != nullptr) {
    callback_(callbackContext_, newState);
  }
}

const char* MatterCaseSession::stateName(CaseState state) {
  switch (state) {
    case CaseState::kIdle: return "idle";
    case CaseState::kSigma1Sent: return "sigma1-sent";
    case CaseState::kSigma1Received: return "sigma1-received";
    case CaseState::kSigma2Sent: return "sigma2-sent";
    case CaseState::kSigma2Received: return "sigma2-received";
    case CaseState::kSigma3Sent: return "sigma3-sent";
    case CaseState::kEstablished: return "established";
    case CaseState::kFailed: return "failed";
    default: return "unknown";
  }
}

}  // namespace xiao_nrf54l15
