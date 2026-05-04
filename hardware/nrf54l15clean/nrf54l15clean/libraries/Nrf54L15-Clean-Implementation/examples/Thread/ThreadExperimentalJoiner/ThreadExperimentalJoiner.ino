
#include <nrf54_all.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core before building this example."
#endif

namespace {

constexpr char kJoinerPskd[] = "THREAD54";
constexpr uint32_t kStatusPrintIntervalMs = 3000U;

xiao_nrf54l15::Nrf54ThreadExperimental g_thread;
uint32_t g_lastStatusPrintMs = 0U;
bool g_printedRole = false;
xiao_nrf54l15::Nrf54ThreadExperimental::Role g_lastRole =
    xiao_nrf54l15::Nrf54ThreadExperimental::Role::kUnknown;
bool g_joinerComplete = false;

void onJoinerCallback(void* context, otError error) {
  Serial.print("thread_joiner joiner_callback error=");
  Serial.println(static_cast<int>(error));
  if (error == OT_ERROR_NONE) {
    g_joinerComplete = true;
    Serial.println("thread_joiner JOIN_SUCCESS — device is now on the Thread network");
    Serial.print("thread_joiner role=");
    Serial.println(g_thread.roleName());
  } else {
    Serial.println("thread_joiner JOIN_FAILED — check PSKd and commissioner");
  }
}

void onStateChanged(void* context, otChangedFlags flags,
                    xiao_nrf54l15::Nrf54ThreadExperimental::Role role) {
  Serial.print("thread_joiner state_changed flags=0x");
  Serial.print(static_cast<unsigned long>(flags), HEX);
  Serial.print(" role=");
  Serial.println(xiao_nrf54l15::Nrf54ThreadExperimental::roleName(role));
}

void printStatus(const char* reason) {
  Serial.print("thread_joiner reason=");
  Serial.println(reason);
  Serial.print("thread_joiner started=");
  Serial.println(g_thread.started() ? 1 : 0);
  Serial.print("thread_joiner attached=");
  Serial.println(g_thread.attached() ? 1 : 0);
  Serial.print("thread_joiner role=");
  Serial.println(g_thread.roleName());
  Serial.print("thread_joiner rloc16=0x");
  Serial.println(g_thread.rloc16(), HEX);
  Serial.print("thread_joiner joiner_active=");
  Serial.println(g_thread.joinerActive() ? 1 : 0);
  Serial.print("thread_joiner joiner_state=");
  Serial.println(g_thread.joinerStateName());
  Serial.print("thread_joiner joiner_complete=");
  Serial.println(g_joinerComplete ? 1 : 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

  Serial.println("thread_joiner Joiner example starting...");
  Serial.print("thread_joiner PSKd=");
  Serial.println(kJoinerPskd);

  const bool beginOk = g_thread.begin(false);
  Serial.print("thread_joiner begin=");
  Serial.println(beginOk ? 1 : 0);
  if (!beginOk) {
    Serial.println("thread_joiner FATAL begin failed");
    return;
  }

  g_thread.setStateChangedCallback(onStateChanged, nullptr);
  printStatus("boot");

  const bool joinerOk = g_thread.startJoiner(kJoinerPskd, nullptr,
                                             onJoinerCallback, nullptr);
  Serial.print("thread_joiner joiner_start=");
  Serial.println(joinerOk ? 1 : 0);
  if (!joinerOk) {
    Serial.print("thread_joiner joiner_error=");
    Serial.println(static_cast<int>(g_thread.lastError()));
  }

  Serial.println("thread_joiner Waiting for commissioner to accept this joiner...");
}

void loop() {
  g_thread.process();

  if (g_joinerComplete) {
    printStatus("joined");
    delay(2000);
    return;
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
