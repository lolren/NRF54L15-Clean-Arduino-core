#include "ble_channel_sounding.h"
#include "nrf54l15_vpr.h"

using namespace xiao_nrf54l15;

namespace {

constexpr uint16_t kConnHandle = 0x0041U;
constexpr uint8_t kRolePeripheral = 1U;
constexpr uint16_t kIntervalUnits = 24U;
constexpr uint16_t kLatency = 0U;
constexpr uint16_t kSupervisionTimeout = 400U;
constexpr uint8_t kPhy1M = 1U;
constexpr uint32_t kEventTimeoutMs = 2500U;
constexpr uint32_t kProbeSummaryMagic = 0x56424348UL;  // "VBCH"
constexpr uint32_t kProbeSummaryVersion = 1U;

struct VprBleConnectionCsHandoffSummary {
  uint32_t magic;
  uint32_t version;
  uint32_t bootCount;
  uint32_t runCount;
  uint32_t completed;
  uint32_t probeOk;
  uint32_t serviceVersionMajor;
  uint32_t serviceVersionMinor;
  uint32_t serviceOpMask;
  uint32_t configuredHandle;
  uint32_t sourceSharedConnected;
  uint32_t sourceSharedHandle;
  uint32_t sourceSharedEventCount;
  uint32_t importedConnected;
  uint32_t importedHandle;
  uint32_t importedEventCount;
  uint32_t handoffPumpCount;
  uint32_t stopPollCount;
  uint32_t readCapsStatus;
  uint32_t defaultStatus;
  uint32_t createStatus;
  uint32_t securityStatus;
  uint32_t setProcedureStatus;
  uint32_t enableStatus;
  uint32_t csReady;
  uint32_t csFailed;
  uint32_t csEstimateValid;
  uint32_t csWorkflowFlags;
  uint32_t csLocalSubevents;
  uint32_t csPeerSubevents;
  uint32_t csLinkSessionOpen;
  uint32_t csLinkHandle;
  uint32_t csLinkConfigId;
  uint32_t csLinkProcedureCounter;
  uint32_t csCompletedProcedureCounter;
  uint32_t csCompletedConfigId;
  uint32_t csDistanceQ4;
};

__attribute__((section(".noinit"))) static VprBleConnectionCsHandoffSummary
    g_probeSummary;

VprSharedTransportStream g_vpr;
VprControllerServiceHost g_service(&g_vpr);
VprControllerServiceCapabilities g_caps{};
VprBleConnectionState g_configuredState{};
VprBleConnectionSharedState g_sourceShared{};
VprBleConnectionEvent g_connectEvent{};
BleCsControllerVprHost g_csHost;
BleCsControllerVprHostConfig g_csConfig{};
VprBleConnectionSharedState g_importedState{};
uint8_t g_handoffPumpCount = 0U;
uint8_t g_stopPollCount = 0U;
uint8_t g_readCapsStatus = 0xFFU;
uint8_t g_defaultStatus = 0xFFU;
uint8_t g_createStatus = 0xFFU;
uint8_t g_securityStatus = 0xFFU;
uint8_t g_setProcedureStatus = 0xFFU;
uint8_t g_enableStatus = 0xFFU;
bool g_lastProbeOk = false;

void initializeProbeSummary() {
  if (g_probeSummary.magic != kProbeSummaryMagic ||
      g_probeSummary.version != kProbeSummaryVersion) {
    memset(&g_probeSummary, 0, sizeof(g_probeSummary));
    g_probeSummary.magic = kProbeSummaryMagic;
    g_probeSummary.version = kProbeSummaryVersion;
  }
  ++g_probeSummary.bootCount;
}

uint32_t packWorkflowFlags(const BleCsControllerWorkflowState& workflow) {
  uint32_t flags = 0U;
  flags |= workflow.remoteCapabilitiesValid ? (1UL << 0U) : 0U;
  flags |= workflow.defaultSettingsApplied ? (1UL << 1U) : 0U;
  flags |= workflow.configCreated ? (1UL << 2U) : 0U;
  flags |= workflow.securityEnabled ? (1UL << 3U) : 0U;
  flags |= workflow.procedureParametersApplied ? (1UL << 4U) : 0U;
  flags |= workflow.procedureEnabled ? (1UL << 5U) : 0U;
  return flags;
}

uint32_t encodeDistanceQ4(float value) {
  if (!(value > 0.0f)) {
    return 0U;
  }
  return static_cast<uint32_t>(value * 10000.0f + 0.5f);
}

void syncProbeSummary(bool completed = false) {
  const BleCsControllerVprHostState& vprState = g_csHost.vprState();
  const BleCsControllerWorkflowState& workflow = g_csHost.workflowState();
  const BleCsControllerSessionState& session = g_csHost.sessionState();

  g_probeSummary.completed = completed ? 1U : 0U;
  g_probeSummary.probeOk = g_lastProbeOk ? 1U : 0U;
  g_probeSummary.serviceVersionMajor = g_caps.serviceVersionMajor;
  g_probeSummary.serviceVersionMinor = g_caps.serviceVersionMinor;
  g_probeSummary.serviceOpMask = g_caps.opMask;
  g_probeSummary.configuredHandle = g_configuredState.connHandle;
  g_probeSummary.sourceSharedConnected = g_sourceShared.connected ? 1U : 0U;
  g_probeSummary.sourceSharedHandle = g_sourceShared.connHandle;
  g_probeSummary.sourceSharedEventCount = g_sourceShared.eventCount;
  g_probeSummary.importedConnected = g_importedState.connected ? 1U : 0U;
  g_probeSummary.importedHandle = g_importedState.connHandle;
  g_probeSummary.importedEventCount = g_importedState.eventCount;
  g_probeSummary.handoffPumpCount = g_handoffPumpCount;
  g_probeSummary.stopPollCount = g_stopPollCount;
  g_probeSummary.readCapsStatus = g_readCapsStatus;
  g_probeSummary.defaultStatus = g_defaultStatus;
  g_probeSummary.createStatus = g_createStatus;
  g_probeSummary.securityStatus = g_securityStatus;
  g_probeSummary.setProcedureStatus = g_setProcedureStatus;
  g_probeSummary.enableStatus = g_enableStatus;
  g_probeSummary.csReady = g_csHost.ready() ? 1U : 0U;
  g_probeSummary.csFailed = g_csHost.failed() ? 1U : 0U;
  g_probeSummary.csEstimateValid = g_csHost.estimateValid() ? 1U : 0U;
  g_probeSummary.csWorkflowFlags = packWorkflowFlags(workflow);
  g_probeSummary.csLocalSubevents = g_csHost.hostState().localSubeventResults;
  g_probeSummary.csPeerSubevents = g_csHost.hostState().peerSubeventResults;
  g_probeSummary.csLinkSessionOpen = vprState.linkSessionOpen ? 1U : 0U;
  g_probeSummary.csLinkHandle = vprState.linkConnHandle;
  g_probeSummary.csLinkConfigId = vprState.linkConfigId;
  g_probeSummary.csLinkProcedureCounter = vprState.linkProcedureCounter;
  g_probeSummary.csCompletedProcedureCounter = session.completedProcedureCounter;
  g_probeSummary.csCompletedConfigId = session.completedConfigId;
  g_probeSummary.csDistanceQ4 = encodeDistanceQ4(session.estimate.distanceMeters);
}

bool ensureGenericService(bool rebootService) {
  if (rebootService && !g_service.bootDefaultService(true)) {
    return false;
  }
  return g_vpr.isRunning() && g_service.readCapabilities(&g_caps);
}

bool runProbe(bool rebootService) {
  g_lastProbeOk = false;
  memset(&g_caps, 0, sizeof(g_caps));
  memset(&g_configuredState, 0, sizeof(g_configuredState));
  memset(&g_sourceShared, 0, sizeof(g_sourceShared));
  memset(&g_connectEvent, 0, sizeof(g_connectEvent));
  g_csHost.reset();
  BleCsControllerVprHost::fillDemoConfig(&g_csConfig);
  memset(&g_importedState, 0, sizeof(g_importedState));
  g_handoffPumpCount = 0U;
  g_stopPollCount = 0U;
  g_readCapsStatus = 0xFFU;
  g_defaultStatus = 0xFFU;
  g_createStatus = 0xFFU;
  g_securityStatus = 0xFFU;
  g_setProcedureStatus = 0xFFU;
  g_enableStatus = 0xFFU;
  syncProbeSummary(false);

  bool ok = ensureGenericService(rebootService);
  ok = ok && g_service.configureBleConnection(
                 kConnHandle, kRolePeripheral, true, kIntervalUnits, kLatency,
                 kSupervisionTimeout, kPhy1M, kPhy1M, &g_configuredState);
  ok = ok && g_service.waitBleConnectionEvent(&g_connectEvent, kEventTimeoutMs);
  ok = ok && g_service.waitBleConnectionSharedState(true, 1U, &g_sourceShared,
                                                    kEventTimeoutMs);
  ok = ok && g_csHost.beginFreshHostFromBleConnection(
                 g_service, g_csConfig, 16U, &g_handoffPumpCount,
                 &g_importedState, kEventTimeoutMs);
  ok = ok && g_csHost.directReadRemoteSupportedCapabilities(&g_readCapsStatus) &&
       g_readCapsStatus == 0U;
  ok = ok && g_csHost.directSetDefaultSettings(
                 g_csConfig.session.workflow.defaultSettings, &g_defaultStatus) &&
       g_defaultStatus == 0U;
  ok = ok && g_csHost.directCreateConfig(g_csConfig.session.workflow.createConfig,
                                         &g_createStatus) &&
       g_createStatus == 0U;
  ok = ok && g_csHost.directSecurityEnable(&g_securityStatus) &&
       g_securityStatus == 0U;
  ok = ok &&
       g_csHost.directSetProcedureParameters(
           g_csConfig.session.workflow.procedureParameters,
           &g_setProcedureStatus) &&
       g_setProcedureStatus == 0U;
  ok = ok && g_csHost.directProcedureEnable(
                 g_csConfig.session.workflow.procedureEnable, &g_enableStatus) &&
       g_enableStatus == 0U;
  while (ok && g_stopPollCount < 24U) {
    if (!g_csHost.poll()) {
      ok = false;
      break;
    }
    ++g_stopPollCount;
    if (g_csHost.sessionState().completedProcedureCounter >= 1U &&
        g_csHost.hostState().localSubeventResults >= 1U &&
        g_csHost.hostState().peerSubeventResults >= 1U &&
        g_csHost.estimateValid()) {
      break;
    }
  }

  const BleCsControllerVprHostState& vprState = g_csHost.vprState();
  const BleCsControllerWorkflowState& workflow = g_csHost.workflowState();
  const BleCsControllerSessionState& session = g_csHost.sessionState();

  g_lastProbeOk =
      ok &&
      (g_caps.opMask & VprControllerServiceHost::kOpBleConnectionConfigure) != 0U &&
      (g_caps.opMask & VprControllerServiceHost::kOpBleConnectionReadState) != 0U &&
      (g_caps.opMask & VprControllerServiceHost::kOpBleConnectionEvent) != 0U &&
      g_sourceShared.connected && g_sourceShared.connHandle == kConnHandle &&
      g_importedState.connected && g_importedState.connHandle == kConnHandle &&
      g_csHost.ready() && !g_csHost.failed() && g_csHost.estimateValid() &&
      vprState.linkSessionOpen && vprState.linkConnHandle == kConnHandle &&
      vprState.linkConfigId ==
          g_csConfig.session.workflow.procedureEnable.configId &&
      workflow.remoteCapabilitiesValid && workflow.defaultSettingsApplied &&
      workflow.configCreated && workflow.securityEnabled &&
      workflow.procedureParametersApplied &&
      g_csHost.hostState().localSubeventResults >= 1U &&
      g_csHost.hostState().peerSubeventResults >= 1U &&
      session.completedProcedureCounter >= 1U &&
      session.completedConfigId ==
          g_csConfig.session.workflow.procedureEnable.configId;

  syncProbeSummary(g_lastProbeOk);
  return g_lastProbeOk;
}

void printStatus() {
  Serial.print("probe_ok=");
  Serial.print(g_lastProbeOk ? 1 : 0);
  Serial.print(" svc=");
  Serial.print(g_caps.serviceVersionMajor);
  Serial.print(".");
  Serial.print(g_caps.serviceVersionMinor);
  Serial.print(" opmask=0x");
  Serial.print(g_caps.opMask, HEX);
  Serial.print(" src=");
  Serial.print(g_sourceShared.connected ? 1 : 0);
  Serial.print("@0x");
  Serial.print(g_sourceShared.connHandle, HEX);
  Serial.print("#");
  Serial.print(g_sourceShared.eventCount);
  Serial.print(" import=");
  Serial.print(g_importedState.connected ? 1 : 0);
  Serial.print("@0x");
  Serial.print(g_importedState.connHandle, HEX);
  Serial.print("#");
  Serial.print(g_importedState.eventCount);
  Serial.print(" cs=");
  Serial.print(g_csHost.ready() ? 1 : 0);
  Serial.print("/");
  Serial.print(g_csHost.failed() ? 1 : 0);
  Serial.print(" link=");
  Serial.print(g_csHost.vprState().linkSessionOpen ? 1 : 0);
  Serial.print("@0x");
  Serial.print(g_csHost.vprState().linkConnHandle, HEX);
  Serial.print(" proc=");
  Serial.print(g_csHost.vprState().linkProcedureCounter);
  Serial.print(" est=");
  Serial.print(g_csHost.estimateValid() ? 1 : 0);
  Serial.print("/");
  Serial.print(g_csHost.sessionState().estimate.distanceMeters, 4);
  Serial.print(" status=");
  Serial.print(g_readCapsStatus, HEX);
  Serial.print("/");
  Serial.print(g_defaultStatus, HEX);
  Serial.print("/");
  Serial.print(g_createStatus, HEX);
  Serial.print("/");
  Serial.print(g_securityStatus, HEX);
  Serial.print("/");
  Serial.print(g_setProcedureStatus, HEX);
  Serial.print("/");
  Serial.print(g_enableStatus, HEX);
  Serial.print(" pumps=");
  Serial.print(g_handoffPumpCount);
  Serial.print("/");
  Serial.println(g_stopPollCount);
}

void printHelp() { Serial.println("Commands: r rerun probe, s status"); }

}  // namespace

void setup() {
  initializeProbeSummary();
  ++g_probeSummary.runCount;
  (void)runProbe(true);
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("VPR BLE connection -> CS handoff probe");
  printHelp();
  Serial.print("probe boot=");
  Serial.println(g_lastProbeOk ? 1 : 0);
  Serial.print("summary_addr=0x");
  Serial.println((uintptr_t)&g_probeSummary, HEX);
  printStatus();
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  const int ch = Serial.read();
  if (ch == 'r' || ch == 'R') {
    ++g_probeSummary.runCount;
    (void)runProbe(true);
    printStatus();
  } else if (ch == 's' || ch == 'S') {
    printStatus();
  } else {
    printHelp();
  }
}
