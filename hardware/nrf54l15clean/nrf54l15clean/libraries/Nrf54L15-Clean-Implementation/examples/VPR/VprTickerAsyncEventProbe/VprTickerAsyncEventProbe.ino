#include "nrf54l15_vpr.h"

using namespace xiao_nrf54l15;

namespace {

constexpr uint32_t kDefaultPeriodTicks = 100000U;
constexpr uint32_t kDefaultStep = 3U;
constexpr uint32_t kDefaultEmitEveryCount = 9U;
constexpr uint32_t kEventTimeoutMs = 2000U;
constexpr uint32_t kQueueProbeEmitEveryCount = 3U;
constexpr uint32_t kQueueProbeSleepMs = 5000U;
constexpr size_t kQueueProbeDrainCount = 3U;

VprSharedTransportStream g_vpr;
VprControllerServiceHost g_service(&g_vpr);
VprControllerServiceCapabilities g_caps{};
VprTickerState g_state{};
VprTickerEvent g_event0{};
VprTickerEvent g_event1{};
VprTickerEvent g_queueEvents[kQueueProbeDrainCount]{};
uint32_t g_appliedEmitEveryCount = 0U;
uint32_t g_dropCount = 0U;
uint32_t g_hostDropCount = 0U;
bool g_lastProbeOk = false;
bool g_lastQueueProbeOk = false;

bool ensureService(bool rebootService) {
  if (rebootService && !g_service.bootDefaultService(true)) {
    return false;
  }
  return g_vpr.isRunning() && g_service.readCapabilities(&g_caps);
}

bool runProbe(bool rebootService) {
  g_lastProbeOk = false;
  memset(&g_caps, 0, sizeof(g_caps));
  memset(&g_state, 0, sizeof(g_state));
  memset(&g_event0, 0, sizeof(g_event0));
  memset(&g_event1, 0, sizeof(g_event1));
  g_appliedEmitEveryCount = 0U;
  g_dropCount = 0U;
  g_hostDropCount = 0U;
  memset(g_queueEvents, 0, sizeof(g_queueEvents));

  if (!ensureService(rebootService)) {
    return false;
  }
  if (!g_service.configureTicker(true, kDefaultPeriodTicks, kDefaultStep, &g_state)) {
    return false;
  }
  if (!g_service.configureTickerEvents(true, kDefaultEmitEveryCount, &g_appliedEmitEveryCount,
                                       &g_dropCount)) {
    return false;
  }
  if (!g_service.waitTickerEvent(&g_event0, kEventTimeoutMs) ||
      !g_service.waitTickerEvent(&g_event1, kEventTimeoutMs) ||
      !g_service.readTickerState(&g_state)) {
    return false;
  }

  g_hostDropCount = g_service.pendingTickerEventDropCount();
  g_lastProbeOk = (g_caps.opMask & VprControllerServiceHost::kOpTickerEventConfigure) != 0U &&
                  g_appliedEmitEveryCount == kDefaultEmitEveryCount &&
                  g_event0.step == kDefaultStep &&
                  g_event1.step == kDefaultStep &&
                  g_event1.sequence > g_event0.sequence &&
                  g_event1.count > g_event0.count &&
                  g_state.enabled &&
                  g_state.count >= g_event1.count &&
                  g_hostDropCount == 0U;
  return g_lastProbeOk;
}

bool runQueueProbe() {
  memset(g_queueEvents, 0, sizeof(g_queueEvents));
  g_lastQueueProbeOk = false;
  g_hostDropCount = 0U;
  if (!ensureService(true)) {
    return false;
  }
  if (!g_service.configureTicker(true, kDefaultPeriodTicks, kDefaultStep, &g_state)) {
    return false;
  }
  if (!g_service.configureTickerEvents(true, kQueueProbeEmitEveryCount, &g_appliedEmitEveryCount,
                                       &g_dropCount)) {
    return false;
  }

  delay(kQueueProbeSleepMs);
  for (size_t i = 0; i < kQueueProbeDrainCount; ++i) {
    if (!g_service.waitTickerEvent(&g_queueEvents[i], kEventTimeoutMs)) {
      return false;
    }
  }
  (void)g_service.readTickerState(&g_state);

  g_hostDropCount = g_service.pendingTickerEventDropCount();
  g_lastQueueProbeOk = g_appliedEmitEveryCount == kQueueProbeEmitEveryCount && g_hostDropCount == 0U;
  for (size_t i = 1; i < kQueueProbeDrainCount; ++i) {
    g_lastQueueProbeOk = g_lastQueueProbeOk && g_queueEvents[i].sequence > g_queueEvents[i - 1].sequence &&
                         g_queueEvents[i].count > g_queueEvents[i - 1].count;
  }
  g_lastQueueProbeOk = g_lastQueueProbeOk && g_queueEvents[0].count != 0U;
  return g_lastQueueProbeOk;
}

void stopTickerEvents() {
  (void)g_service.configureTickerEvents(false, 0U, &g_appliedEmitEveryCount, &g_dropCount);
  (void)g_service.configureTicker(false, 0U, 1U, &g_state);
}

void printStatus() {
  Serial.print("probe_ok=");
  Serial.print(g_lastProbeOk ? 1 : 0);
  Serial.print(" running=");
  Serial.print(g_vpr.isRunning() ? 1 : 0);
  Serial.print(" svc=");
  Serial.print(g_caps.serviceVersionMajor);
  Serial.print(".");
  Serial.print(g_caps.serviceVersionMinor);
  Serial.print(" opmask=0x");
  Serial.print(g_caps.opMask, HEX);
  Serial.print(" ticker_en=");
  Serial.print(g_state.enabled ? 1 : 0);
  Serial.print(" period=");
  Serial.print(g_state.periodTicks);
  Serial.print(" step=");
  Serial.print(g_state.step);
  Serial.print(" emit_every=");
  Serial.print(g_appliedEmitEveryCount);
  Serial.print(" drop_count=");
  Serial.print(g_dropCount);
  Serial.print(" host_drop=");
  Serial.print(g_hostDropCount);
  Serial.print(" ev0=");
  Serial.print(g_event0.count);
  Serial.print("@");
  Serial.print(g_event0.heartbeat);
  Serial.print("#");
  Serial.print(g_event0.sequence);
  Serial.print(" ev1=");
  Serial.print(g_event1.count);
  Serial.print("@");
  Serial.print(g_event1.heartbeat);
  Serial.print("#");
  Serial.print(g_event1.sequence);
  Serial.print(" live_count=");
  Serial.print(g_state.count);
  Serial.print(" queue_ok=");
  Serial.println(g_lastQueueProbeOk ? 1 : 0);
}

void printNextEvent() {
  VprTickerEvent event{};
  if (!g_service.waitTickerEvent(&event, kEventTimeoutMs)) {
    Serial.println("event wait failed");
    return;
  }
  Serial.print("event flags=0x");
  Serial.print(event.flags, HEX);
  Serial.print(" count=");
  Serial.print(event.count);
  Serial.print(" step=");
  Serial.print(event.step);
  Serial.print(" heartbeat=");
  Serial.print(event.heartbeat);
  Serial.print(" seq=");
  Serial.println(event.sequence);
}

void printQueueProbe() {
  Serial.print("queue_probe=");
  Serial.println(runQueueProbe() ? 1 : 0);
  for (size_t i = 0; i < kQueueProbeDrainCount; ++i) {
    Serial.print("queue_ev");
    Serial.print(i);
    Serial.print("=");
    Serial.print(g_queueEvents[i].count);
    Serial.print("@");
    Serial.print(g_queueEvents[i].heartbeat);
    Serial.print("#");
    Serial.println(g_queueEvents[i].sequence);
  }
  printStatus();
}

void printHelp() {
  Serial.println("Commands: r rerun async event probe, q run queued burst probe, e wait next event, x stop ticker/events, s status");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("VPR ticker async event probe");
  printHelp();
  Serial.print("probe boot=");
  Serial.println(runProbe(true) ? 1 : 0);
  printStatus();
}

void loop() {
  if (!Serial.available()) {
    delay(20);
    return;
  }
  const int incoming = Serial.read();
  if (incoming < 0) {
    return;
  }

  switch (static_cast<char>(incoming)) {
    case 'r':
      Serial.print("probe run=");
      Serial.println(runProbe(false) ? 1 : 0);
      printStatus();
      break;
    case 'e':
      printNextEvent();
      break;
    case 'q':
      printQueueProbe();
      break;
    case 'x':
      stopTickerEvents();
      Serial.println("ticker events stopped");
      printStatus();
      break;
    case 's':
      printStatus();
      break;
    default:
      printHelp();
      break;
  }
}
