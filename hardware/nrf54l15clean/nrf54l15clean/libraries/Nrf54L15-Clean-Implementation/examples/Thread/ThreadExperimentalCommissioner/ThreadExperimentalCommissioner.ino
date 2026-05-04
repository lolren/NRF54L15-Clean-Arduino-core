
#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core before building this example."
#endif

namespace {

constexpr char kCommissionerPskd[] = "THREAD54";
constexpr uint32_t kStatusPrintIntervalMs = 3000U;

xiao_nrf54l15::Nrf54ThreadExperimental g_thread;
uint32_t g_lastStatusPrintMs = 0U;
xiao_nrf54l15::Nrf54ThreadExperimental::Role g_lastRole =
    xiao_nrf54l15::Nrf54ThreadExperimental::Role::kUnknown;
bool g_commissionerStarted = false;
bool g_joinerConnected = false;

void onCommissionerJoinerCallback(void* context,
                                  const otExtAddress* joinerId,
                                  otError error) {
  if (joinerId != nullptr) {
    char eui64Hex[OT_EXT_ADDRESS_SIZE * 2 + 1] = {0};
    for (size_t i = 0; i < OT_EXT_ADDRESS_SIZE; ++i) {
      snprintf(&eui64Hex[i * 2], 3, "%02X",
               static_cast<unsigned>(joinerId->m8[i]));
    }
    Serial.print("thread_commissioner joiner_event eui64=");
    Serial.println(eui64Hex);
  }

  Serial.print("thread_commissioner joiner_event error=");
  Serial.println(static_cast<int>(error));
  if (error == OT_ERROR_NONE) {
    g_joinerConnected = true;
    Serial.println("thread_commissioner JOINER_ACCEPTED — device was commissioned");
  }
}

void onCommissionerStateCallback(void* context,
                                 otCommissionerState state) {
  Serial.print("thread_commissioner state_changed state=");
  Serial.println(g_thread.commissionerStateName());
}

void onStateChanged(void* context, otChangedFlags flags,
                    xiao_nrf54l15::Nrf54ThreadExperimental::Role role) {
  Serial.print("thread_commissioner thread_state flags=0x");
  Serial.print(static_cast<unsigned long>(flags), HEX);
  Serial.print(" role=");
  Serial.println(xiao_nrf54l15::Nrf54ThreadExperimental::roleName(role));

  if (role == xiao_nrf54l15::Nrf54ThreadExperimental::Role::kLeader &&
      !g_commissionerStarted) {
    Serial.println("thread_commissioner LEADER detected — starting commissioner...");
    const bool ok = g_thread.startCommissioner();
    Serial.print("thread_commissioner commissioner_start=");
    Serial.println(ok ? 1 : 0);
    if (ok) {
      g_commissionerStarted = true;
      g_thread.setCommissionerStateCallback(onCommissionerStateCallback, nullptr);
      g_thread.setCommissionerJoinerCallback(onCommissionerJoinerCallback, nullptr);

      const bool added = g_thread.addJoinerToCommissioner(kCommissionerPskd);
      Serial.print("thread_commissioner joiner_added=");
      Serial.println(added ? 1 : 0);
      Serial.print("thread_commissioner pskd=");
      Serial.println(kCommissionerPskd);
      Serial.println(
          "thread_commissioner Waiting for joiner with this PSKd...");
    }
  }
}

void printStatus(const char* reason) {
  Serial.print("thread_commissioner reason=");
  Serial.println(reason);
  Serial.print("thread_commissioner started=");
  Serial.println(g_thread.started() ? 1 : 0);
  Serial.print("thread_commissioner attached=");
  Serial.println(g_thread.attached() ? 1 : 0);
  Serial.print("thread_commissioner role=");
  Serial.println(g_thread.roleName());
  Serial.print("thread_commissioner rloc16=0x");
  Serial.println(g_thread.rloc16(), HEX);
  Serial.print("thread_commissioner commissioner_active=");
  Serial.println(g_thread.commissionerActive() ? 1 : 0);
  Serial.print("thread_commissioner commissioner_state=");
  Serial.println(g_thread.commissionerStateName());
  if (g_thread.commissionerActive()) {
    uint16_t sessionId = 0U;
    if (g_thread.commissionerSessionId(&sessionId)) {
      Serial.print("thread_commissioner commissioner_session_id=");
      Serial.println(sessionId);
    }
  }
  Serial.print("thread_commissioner joiner_connected=");
  Serial.println(g_joinerConnected ? 1 : 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

  Serial.println("thread_commissioner Commissioner example starting...");
  Serial.print("thread_commissioner PSKd=");
  Serial.println(kCommissionerPskd);

  const bool beginOk = g_thread.begin(false);
  Serial.print("thread_commissioner begin=");
  Serial.println(beginOk ? 1 : 0);
  if (!beginOk) {
    Serial.println("thread_commissioner FATAL begin failed");
    return;
  }

  g_thread.setStateChangedCallback(onStateChanged, nullptr);
  g_thread.setCommissionerJoinerCallback(onCommissionerJoinerCallback, nullptr);

  otOperationalDataset dataset = {};
  xiao_nrf54l15::Nrf54ThreadExperimental::buildDemoDataset(&dataset);
  const bool setOk = g_thread.setActiveDataset(dataset);
  Serial.print("thread_commissioner dataset_set=");
  Serial.println(setOk ? 1 : 0);

  printStatus("boot");
  Serial.println("thread_commissioner Will become Leader, then start Commissioner...");
}

void loop() {
  g_thread.process();

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
