#pragma once

#include <stddef.h>
#include <stdint.h>

#include "matter_foundation_target.h"
#include "matter_platform_stage.h"
#include "matter_manual_pairing.h"
#include "matter_secp256r1.h"
#include "matter_pbkdf2.h"

namespace xiao_nrf54l15 {

// SPAKE2+ cryptographic constants for Matter PASE commissioning
constexpr size_t kMatterSpake2pHashSize = 32U;
constexpr size_t kMatterSpake2pWScalarSize = 32U;    // w0, w1 are scalars
constexpr size_t kMatterSpake2pW0Length = 32U;
constexpr size_t kMatterSpake2pW1Length = 32U;
constexpr size_t kMatterSpake2pPointSize = 65U;      // Uncompressed secp256r1
constexpr size_t kMatterSpake2pConfirmationSize = 32U;
constexpr size_t kMatterSpake2pSaltSize = 32U;
constexpr uint32_t kMatterSpake2pPbkdf2Iterations = 2000U;

// CHIP message framing constants
enum class MatterMessageExchangeFlags : uint8_t {
  kNone = 0x00U,
  kInitiator = 0x01U,
  kAck = 0x02U,
  kReliable = 0x04U,
  kDuplicate = 0x08U,
};

enum class MatterMessageProtocol : uint16_t {
  kSecureChannel = 0x0000U,
  kInteractionModel = 0x0001U,
  kBdx = 0x0002U,
  kUserDirectedCommissioning = 0x0003U,
  kEcho = 0x0004U,
};

enum class MatterMessageType : uint8_t {
  // Secure Channel
  kMsgCounterSyncReq = 0x00U,
  kMsgCounterSyncResp = 0x01U,
  kStandaloneAck = 0x10U,
  kPBKDFParamRequest = 0x20U,
  kPBKDFParamResponse = 0x21U,
  kPaseSpake2p1 = 0x22U,
  kPaseSpake2p2 = 0x23U,
  kPaseSpake2p3 = 0x24U,
  kPaseSpake2pError = 0x2FU,
  kSigma1 = 0x30U,
  kSigma2 = 0x31U,
  kSigma3 = 0x32U,
  kSigma2Resume = 0x33U,
  kStatusReport = 0x40U,

  // Interaction Model
  kReadRequest = 0x02U,
  kReportData = 0x05U,
  kInvokeCommandRequest = 0x08U,
  kInvokeCommandResponse = 0x09U,
  kTimedRequest = 0x0AU,
};

// Commissioning flow state
enum class MatterCommissioningState : uint8_t {
  kIdle = 0U,
  kDiscovering = 1U,
  kPasePbkdfParamsSent = 2U,
  kPaseSpake2pInProgress = 3U,
  kPaseComplete = 4U,
  kSigmaInProgress = 5U,
  kNocSent = 6U,
  kCommissioned = 7U,
  kFailed = 8U,
};

// CHIP message header
struct MatterMessageHeader {
  uint8_t exchangeFlags = 0U;
  uint8_t sessionType = 0U;
  uint8_t securityFlags = 0U;
  uint16_t messageId = 0U;
  uint32_t sourceNodeId = 0U;
  uint32_t destNodeId = 0U;
  uint16_t exchangeId = 0U;
  uint16_t protocolId = 0U;
  uint8_t  protocolOpcode = 0U;
  uint16_t ackedMessageId = 0U;
};

// PBKDF param structures
struct MatterPbkdfParamRequest {
  uint8_t initiatorRandom[32] = {0};
  uint16_t initiatorSessionId = 0U;
  uint16_t passcodeId = 0U;
  uint8_t hasPbkdfParameters = 0;
  uint32_t idleRetransTimeoutMs = 0U;
  uint32_t activeRetransTimeoutMs = 0U;
};

struct MatterPbkdfParamResponse {
  uint8_t responderRandom[32] = {0};
  uint16_t responderSessionId = 0U;
  uint16_t passcodeId = 0U;
  uint8_t hasPbkdfParameters = 0;
  uint32_t idleRetransTimeoutMs = 0U;
  uint32_t activeRetransTimeoutMs = 0U;
};

// SPAKE2+ verifier state: w0, L = w1 * G
struct MatterSpake2pVerifier {
  uint8_t w0[kMatterSpake2pW0Length] = {0};
  uint8_t L[kMatterSpake2pPointSize] = {0};  // L = w1 * G (uncompressed)
  uint8_t salt[kMatterSpake2pSaltSize] = {0};
  uint32_t iterations = kMatterSpake2pPbkdf2Iterations;
  bool valid = false;
};

// Session state for PASE commissioning
struct MatterPaseSessionState {
  bool active = false;
  bool initiator = false;
  MatterCommissioningState state = MatterCommissioningState::kIdle;
  uint16_t localSessionId = 0U;
  uint16_t peerSessionId = 0U;
  uint16_t passcodeId = 0U;
  uint32_t setupPinCode = 0U;

  // SPAKE2+ derived keys
  uint8_t w0[kMatterSpake2pW0Length] = {0};    // Verifier's w0
  uint8_t w1[kMatterSpake2pW1Length] = {0};    // Prover's w1
  uint8_t ws[kMatterSpake2pHashSize] = {0};    // From PBKDF2
  uint8_t L[kMatterSpake2pPointSize] = {0};    // L = w1 * G (public)
  uint8_t salt[kMatterSpake2pSaltSize] = {0};
  uint32_t pbkdf2Iterations = kMatterSpake2pPbkdf2Iterations;

  // ECC points (uncompressed format)
  uint8_t X[kMatterSpake2pPointSize] = {0};    // P's X = x*G + w0*G
  uint8_t Y[kMatterSpake2pPointSize] = {0};    // V's Y = y*G + w0*G
  uint8_t Z[kMatterSpake2pPointSize] = {0};    // Shared: Z = x*(Y-w0*G) = y*(X-w0*G)
  uint8_t V[kMatterSpake2pPointSize] = {0};    // V = w1*(Y-w0*G) = w1*(X-w0*G)

  // Shared secret and keys
  uint8_t sharedSecret[kMatterSpake2pHashSize] = {0};
  uint8_t ke[kMatterSpake2pHashSize] = {0};     // Encryption key

  // Exchange randoms
  uint8_t initiateRandom[32] = {0};
  uint8_t respondRandom[32] = {0};

  // Confirmations
  uint8_t cA[kMatterSpake2pConfirmationSize] = {0};
  uint8_t cB[kMatterSpake2pConfirmationSize] = {0};
};

class MatterPaseCommissioning {
 public:
  using CommissioningCallback =
      void (*)(void* context, MatterCommissioningState state,
               uint32_t errorCode);

  MatterPaseCommissioning() = default;

  // Commissionee (prover): the device being commissioned
  bool beginAsCommissionee(MatterPlatform* platform,
                           CommissioningCallback callback = nullptr,
                           void* context = nullptr);

  // Commissioner (verifier): the device doing the commissioning
  bool beginAsCommissioner(MatterPlatform* platform,
                           CommissioningCallback callback = nullptr,
                           void* context = nullptr);

  void end();
  void process();
  bool active() const;
  MatterCommissioningState state() const;
  const char* stateName() const;

  // Set the setup PIN code for PASE
  bool setPasscode(uint32_t passcode);
  bool setDiscriminator(uint16_t discriminator);

  // Derive SPAKE2+ verifier from passcode (call on commissioner)
  static bool deriveVerifier(uint32_t passcode,
                             const uint8_t salt[kMatterSpake2pSaltSize],
                             uint32_t iterations,
                             MatterSpake2pVerifier* outVerifier);

  // Send PBKDF param request (commissioner initiates)
  bool sendPbkdfParamRequest(const otIp6Address& peerAddr,
                             uint16_t peerPort,
                             uint32_t setupPinCode);

  // Send PBKDF param response (commissionee responds)
  bool sendPbkdfParamResponse(const otIp6Address& peerAddr,
                              uint16_t peerPort);

  // Initiate SPAKE2+ from commissioner side (after PBKDF params exchanged)
  bool initiateSpake2p(const otIp6Address& peerAddr, uint16_t peerPort);

  // Get session shared secret (available after PASE complete)
  bool getSharedSecret(uint8_t outSharedSecret[kMatterSpake2pHashSize]) const;

  static const char* stateName(MatterCommissioningState state);

 private:
  static void handleUdpReceive(void* context,
                               const uint8_t* payload, uint16_t length,
                               const otIp6Address& source,
                               uint16_t sourcePort);

  void handleMessage(const uint8_t* payload, uint16_t length,
                     const otIp6Address& source, uint16_t sourcePort);
  void handlePbkdfParamRequest(const uint8_t* payload, uint16_t length,
                               const otIp6Address& source, uint16_t sourcePort);
  void handlePbkdfParamResponse(const uint8_t* payload, uint16_t length,
                                const otIp6Address& source, uint16_t sourcePort);
  void handleSpake2p1(const uint8_t* payload, uint16_t length,
                      const otIp6Address& source, uint16_t sourcePort);
  void handleSpake2p2(const uint8_t* payload, uint16_t length,
                      const otIp6Address& source, uint16_t sourcePort);
  void handleSpake2p3(const uint8_t* payload, uint16_t length,
                      const otIp6Address& source, uint16_t sourcePort);

  // SPAKE2+ cryptographic operations
  bool deriveW0W1FromPasscode();
  bool computeSpake2pX();
  bool computeSpake2pY();
  bool computeSpake2pZ(bool prover);
  bool deriveSharedSecret();
  bool verifyConfirmationB();
  bool verifyConfirmationA();
  bool generateConfirmationA();
  bool generateConfirmationB();

  // Message framing
  bool parseMessageHeader(const uint8_t* payload, uint16_t length,
                          MatterMessageHeader* outHeader,
                          size_t* outPayloadOffset = nullptr) const;
  bool buildMessageHeader(const MatterMessageHeader& header,
                          uint8_t* outBuffer, size_t outCapacity,
                          size_t* outLength = nullptr) const;
  bool sendMessage(const otIp6Address& peerAddr, uint16_t peerPort,
                   const MatterMessageHeader& header,
                   const uint8_t* appPayload, uint16_t appPayloadLength);

  uint16_t nextExchangeId();
  uint16_t nextMessageId();
  void generateRandom(uint8_t* output, size_t length);
  void advanceState(MatterCommissioningState newState,
                    uint32_t errorCode = 0U);

  MatterPlatform* platform_ = nullptr;
  CommissioningCallback callback_ = nullptr;
  void* callbackContext_ = nullptr;
  bool initiator_ = false;

  uint16_t localExchangeId_ = 0U;
  uint16_t peerExchangeId_ = 0U;
  uint16_t localMessageId_ = 0U;
  uint16_t peerMessageId_ = 0U;
  uint16_t peerPort_ = 0U;
  otIp6Address peerAddr_ = {};

  MatterPaseSessionState session_ = {};
  uint32_t setupPinCode_ = 20202021UL;
  uint16_t discriminator_ = 3840U;

  // Pre-computed verifier (commissioner side)
  MatterSpake2pVerifier verifier_ = {};
};

}  // namespace xiao_nrf54l15
