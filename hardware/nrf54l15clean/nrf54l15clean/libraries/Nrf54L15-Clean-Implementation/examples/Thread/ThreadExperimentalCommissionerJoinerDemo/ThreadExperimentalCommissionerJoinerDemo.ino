
#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core before building this example."
#endif

// Two-board commissioner+joiner demo.
//
// Board A: flash with ROLE=COMMISSIONER.  Board A forms a Thread network,
//          becomes Leader, starts a Commissioner, and adds a Joiner entry.
// Board B: flash with ROLE=JOINER.  Board B starts a Joiner and waits to
//          be discovered and provisioned by Board A.
//
// After a successful join, both boards can communicate over Thread UDP.

namespace {

constexpr char kCommissionerPskd[] = "THREAD54";

enum class DeviceRole : uint8_t {
  kCommissioner = 0U,
  kJoiner = 1U,
};

// Change this to kJoiner for the second board.
constexpr DeviceRole kMyRole = DeviceRole::kCommissioner;

constexpr uint32_t kStatusPrintIntervalMs = 3000U;
constexpr uint16_t kDemoUdpPort = 12345U;

xiao_nrf54l15::Nrf54ThreadExperimental g_thread;
uint32_t g_lastStatusPrintMs = 0U;
xiao_nrf54l15::Nrf54ThreadExperimental::Role g_lastRole =
    xiao_nrf54l15::Nrf54ThreadExperimental::Role::kUnknown;
bool g_commissionerStarted = false;
bool g_joinerStarted = false;
bool g_joinerComplete = false;
bool g_udpOpened = false;
char g_lastUdpPayload[128] = {0};
uint16_t g_lastUdpLength = 0U;

// Commissioner callbacks
void onCommissionerJoinerCallback(void* context,
                                  const otExtAddress* joinerId,
                                  otError error) {
  if (joinerId != nullptr) {
    char eui64Hex[OT_EXT_ADDRESS_SIZE * 2 + 1] = {0};
    for (size_t i = 0; i < OT_EXT_ADDRESS_SIZE; ++i) {
      snprintf(&eui64Hex[i * 2], 3, "%02X",
               static_cast<unsigned>(joinerId->m8[i]));
    }
    Serial.print("thread_cj_demo commissioner_joiner_eui64=");
    Serial.println(eui64Hex);
  }
  Serial.print("thread_cj_demo commissioner_joiner_error=");
  Serial.println(static_cast<int>(error));
}

void onCommissionerStateCallback(void* context,
                                 otCommissionerState state) {
  Serial.print("thread_cj_demo commissioner_state=");
  Serial.println(g_thread.commissionerStateName());
  if (state == OT_COMMISSIONER_STATE_ACTIVE) {
    g_thread.setCommissionerProvisioningUrl("https://nrf54l15.local");
  }
}

// Joiner callbacks
void onJoinerCallback(void* context, otError error) {
  Serial.print("thread_cj_demo joiner_result error=");
  Serial.println(static_cast<int>(error));
  if (error == OT_ERROR_NONE) {
    g_joinerComplete = true;
    Serial.println("thread_cj_demo JOIN_SUCCESS");
  } else {
    Serial.println("thread_cj_demo JOIN_FAILED");
  }
}

// Common
void onStateChanged(void* context, otChangedFlags flags,
                    xiao_nrf54l15::Nrf54ThreadExperimental::Role role) {
  Serial.print("thread_cj_demo thread_state flags=0x");
  Serial.print(static_cast<unsigned long>(flags), HEX);
  Serial.print(" role=");
  Serial.println(xiao_nrf54l15::Nrf54ThreadExperimental::roleName(role));

  if (kMyRole == DeviceRole::kCommissioner &&
      role == xiao_nrf54l15::Nrf54ThreadExperimental::Role::kLeader &&
      !g_commissionerStarted) {
    if (!g_thread.commissionerSupported()) {
      g_commissionerStarted = true;
      Serial.println(
          "thread_cj_demo Standard MeshCoP Commissioner is not compiled in "
          "this staged core yet.");
      Serial.println(
          "thread_cj_demo Use ThreadExperimentalJoinerPSK* for the current "
          "two-board join path.");
      return;
    }

    Serial.println("thread_cj_demo LEADER — starting commissioner...");
    const bool ok = g_thread.startCommissioner();
    Serial.print("thread_cj_demo commissioner_start=");
    Serial.println(ok ? 1 : 0);
    if (ok) {
      g_commissionerStarted = true;
      g_thread.setCommissionerStateCallback(onCommissionerStateCallback, nullptr);
      g_thread.setCommissionerJoinerCallback(onCommissionerJoinerCallback, nullptr);
      const bool added = g_thread.addJoinerToCommissioner(kCommissionerPskd);
      Serial.print("thread_cj_demo joiner_entry_added=");
      Serial.println(added ? 1 : 0);
    }
  }
}

void onUdpReceive(void* context, const uint8_t* payload, uint16_t length,
                  const otMessageInfo& messageInfo) {
  if (payload == nullptr || length == 0U) {
    return;
  }

  const uint16_t copyLength =
      (length < sizeof(g_lastUdpPayload) - 1U) ? length
                                                : sizeof(g_lastUdpPayload) - 1U;
  memcpy(g_lastUdpPayload, payload, copyLength);
  g_lastUdpPayload[copyLength] = '\0';
  g_lastUdpLength = copyLength;

  Serial.print("thread_cj_demo udp_rx from=");
  for (size_t i = 0; i < 6; ++i) {
    if (i > 0) {
      Serial.print(':');
    }
    Serial.print(messageInfo.mPeerAddr.mFields.m16[i], HEX);
  }
  Serial.print(" payload=");
  Serial.println(g_lastUdpPayload);

  // Echo back
  g_thread.sendUdp(messageInfo.mPeerAddr, messageInfo.mPeerPort, payload,
                   length);
}

void tryOpenUdp() {
  if (g_udpOpened) {
    return;
  }

  const bool opened = g_thread.openUdp(kDemoUdpPort, onUdpReceive, nullptr);
  Serial.print("thread_cj_demo udp_open=");
  Serial.println(opened ? 1 : 0);
  if (opened) {
    g_udpOpened = true;
  }
}

void trySendTestMessage() {
  if (!g_udpOpened) {
    return;
  }

  otIp6Address leaderAddr = {};
  if (!g_thread.getLeaderRloc(&leaderAddr)) {
    return;
  }

  const char* message = "Hello from Thread CJ Demo!";
  const uint16_t messageLength = static_cast<uint16_t>(strlen(message));
  const bool sent = g_thread.sendUdp(leaderAddr, kDemoUdpPort, message,
                                     messageLength);
  if (sent) {
    Serial.print("thread_cj_demo udp_tx msg=");
    Serial.println(message);
  }
}

void printStatus(const char* reason) {
  Serial.print("thread_cj_demo reason=");
  Serial.println(reason);
  Serial.print("thread_cj_demo role=");
  Serial.println(kMyRole == DeviceRole::kCommissioner ? "commissioner"
                                                       : "joiner");
  Serial.print("thread_cj_demo thread_role=");
  Serial.println(g_thread.roleName());
  Serial.print("thread_cj_demo rloc16=0x");
  Serial.println(g_thread.rloc16(), HEX);
  Serial.print("thread_cj_demo attached=");
  Serial.println(g_thread.attached() ? 1 : 0);

  if (kMyRole == DeviceRole::kCommissioner) {
    Serial.print("thread_cj_demo commissioner_supported=");
    Serial.println(g_thread.commissionerSupported() ? 1 : 0);
    Serial.print("thread_cj_demo commissioner_active=");
    Serial.println(g_thread.commissionerActive() ? 1 : 0);
    Serial.print("thread_cj_demo commissioner_state=");
    Serial.println(g_thread.commissionerStateName());
    uint16_t sessionId = 0U;
    if (g_thread.commissionerSessionId(&sessionId)) {
      Serial.print("thread_cj_demo commissioner_session_id=");
      Serial.println(sessionId);
    }
  } else {
    Serial.print("thread_cj_demo joiner_supported=");
    Serial.println(g_thread.joinerSupported() ? 1 : 0);
    Serial.print("thread_cj_demo joiner_active=");
    Serial.println(g_thread.joinerActive() ? 1 : 0);
    Serial.print("thread_cj_demo joiner_state=");
    Serial.println(g_thread.joinerStateName());
  }

  Serial.print("thread_cj_demo joiner_complete=");
  Serial.println(g_joinerComplete ? 1 : 0);
  Serial.print("thread_cj_demo udp_opened=");
  Serial.println(g_udpOpened ? 1 : 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

  Serial.println("thread_cj_demo === Commissioner+Joiner 2-Board Demo ===");
  Serial.print("thread_cj_demo my_role=");
  Serial.println(kMyRole == DeviceRole::kCommissioner ? "commissioner"
                                                       : "joiner");
  Serial.print("thread_cj_demo pskd=");
  Serial.println(kCommissionerPskd);
  Serial.print("thread_cj_demo standard_meshcop_enabled=");
  Serial.println((g_thread.commissionerSupported() && g_thread.joinerSupported())
                     ? 1
                     : 0);

  const bool beginOk = g_thread.begin(false);
  Serial.print("thread_cj_demo begin=");
  Serial.println(beginOk ? 1 : 0);
  if (!beginOk) {
    return;
  }

  g_thread.setStateChangedCallback(onStateChanged, nullptr);

  if (kMyRole == DeviceRole::kCommissioner) {
    // Board A: form network with demo dataset
    otOperationalDataset dataset = {};
    xiao_nrf54l15::Nrf54ThreadExperimental::buildDemoDataset(&dataset);
    g_thread.setActiveDataset(dataset);
    Serial.println(
        "thread_cj_demo Commissioner will form network, become Leader, then "
        "start Commissioner...");
  } else {
    // Board B: start joiner
    if (!g_thread.joinerSupported()) {
      Serial.println(
          "thread_cj_demo Standard MeshCoP Joiner is not compiled in this "
          "staged core yet.");
      Serial.println(
          "thread_cj_demo Use ThreadExperimentalJoinerPSK* for the current "
          "two-board join path.");
      printStatus("joiner-not-compiled");
      return;
    }

    const bool joinerOk = g_thread.startJoiner(kCommissionerPskd, nullptr,
                                               onJoinerCallback, nullptr);
    Serial.print("thread_cj_demo joiner_start=");
    Serial.println(joinerOk ? 1 : 0);
    if (!joinerOk) {
      Serial.print("thread_cj_demo joiner_error=");
      Serial.println(static_cast<int>(g_thread.lastError()));
    } else {
      g_joinerStarted = true;
      Serial.println("thread_cj_demo Waiting for commissioner...");
    }
  }

  printStatus("boot");
}

void loop() {
  g_thread.process();

  // After join complete on joiner side, open UDP for testing
  if (kMyRole == DeviceRole::kJoiner && g_joinerComplete) {
    tryOpenUdp();
  }

  // Commissioner side: also open UDP for testing once attached
  if (kMyRole == DeviceRole::kCommissioner && g_thread.attached()) {
    tryOpenUdp();
  }

  const xiao_nrf54l15::Nrf54ThreadExperimental::Role currentRole =
      g_thread.role();
  if (currentRole != g_lastRole) {
    g_lastRole = currentRole;
    printStatus("role-change");
  }

  if ((millis() - g_lastStatusPrintMs) >= kStatusPrintIntervalMs) {
    g_lastStatusPrintMs = millis();
    printStatus("heartbeat");
  }
}
