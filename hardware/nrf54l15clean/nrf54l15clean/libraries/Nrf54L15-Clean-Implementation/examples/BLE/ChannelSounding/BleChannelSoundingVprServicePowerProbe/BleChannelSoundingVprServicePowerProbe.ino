/*
 * BleChannelSoundingVprServicePowerProbe
 *
 * Low-noise single-board measurement harness for bench current capture of the
 * generic VPR BLE -> CS runtime. This sketch is intentionally quiet while the
 * measured phases are running, then prints one summary line after the run
 * completes so you can collect serial output after the current capture.
 *
 * Phase order:
 *   1. generic-service idle (no BLE link)
 *   2. encrypted BLE link connected, no CS workflow
 *   3. encrypted BLE link with repeated nominal CS workflow runs
 *   4. generic-service idle again after disconnect
 *
 * The printed `last_q4` and derived distance remain nominal synthetic CS
 * regression output only. This sketch is for current characterization, not
 * physical ranging validation.
 */

#include <Arduino.h>

#include "nrf54l15_vpr.h"

using namespace xiao_nrf54l15;

namespace {

constexpr uint16_t kConnHandle = 0x0041U;
constexpr uint8_t kRolePeripheral = 1U;
constexpr uint16_t kIntervalUnits = 24U;
constexpr uint16_t kLatency = 0U;
constexpr uint16_t kSupervisionTimeout = 400U;
constexpr uint8_t kPhy1M = 1U;
constexpr uint8_t kConfigId = 1U;
constexpr uint8_t kMaxProcedureCount = 3U;
constexpr uint8_t kDisconnectReason = 0x13U;
constexpr uint32_t kPhaseServiceIdleMs = 5000UL;
constexpr uint32_t kPhaseConnectedMs = 5000UL;
constexpr uint32_t kPhaseCsMs = 10000UL;
constexpr uint32_t kPhaseFinalIdleMs = 5000UL;
constexpr uint32_t kCsInterRunDelayMs = 250UL;
constexpr uint32_t kSummaryRepeatMs = 2000UL;
constexpr uint32_t kTimeoutMs = 5000UL;

// Optional bench marker. Keep this disabled for the cleanest current capture.
constexpr bool kEnableMarkerPin = false;
constexpr uint8_t kMarkerPin = PIN_D10;
constexpr uint16_t kMarkerPulseMs = 2U;

VprSharedTransportStream g_vpr;
VprControllerServiceHost g_service(&g_vpr);

struct PowerProbeSummary {
  bool ok = false;
  uint8_t serviceVersionMajor = 0U;
  uint8_t serviceVersionMinor = 0U;
  uint32_t opMask = 0UL;
  uint32_t phaseServiceIdleMs = 0UL;
  uint32_t phaseConnectedMs = 0UL;
  uint32_t phaseCsMs = 0UL;
  uint32_t phaseFinalIdleMs = 0UL;
  uint32_t totalRunMs = 0UL;
  uint32_t csRunCount = 0UL;
  uint32_t completedProcedureCount = 0UL;
  uint16_t lastNominalDistanceQ4 = 0U;
  uint8_t lastLocalSubeventCount = 0U;
  uint8_t lastPeerSubeventCount = 0U;
  uint8_t lastLocalStepCount = 0U;
  uint8_t lastPeerStepCount = 0U;
  bool finalConnected = false;
  bool finalLinkBound = false;
  bool finalLinkRunnable = false;
  bool finalWorkflowConfigured = false;
  bool finalWorkflowEnabled = false;
  uint8_t finalDisconnectReason = 0U;
};

PowerProbeSummary g_summary;
bool g_summaryReady = false;
uint32_t g_lastSummaryPrintMs = 0UL;

void markerPulse(uint8_t count) {
  if (!kEnableMarkerPin) {
    return;
  }
  for (uint8_t i = 0; i < count; ++i) {
    digitalWrite(kMarkerPin, HIGH);
    delay(kMarkerPulseMs);
    digitalWrite(kMarkerPin, LOW);
    delay(kMarkerPulseMs);
  }
}

bool waitConnectedState(bool connected, VprBleConnectionSharedState* state) {
  return g_service.waitBleConnectionSharedState(connected, 1U, state, kTimeoutMs);
}

bool configureEncryptedLink(VprBleConnectionState* configured,
                            VprBleConnectionSharedState* shared) {
  if (!g_service.configureBleConnection(kConnHandle, kRolePeripheral, true,
                                        kIntervalUnits, kLatency,
                                        kSupervisionTimeout, kPhy1M, kPhy1M,
                                        configured)) {
    return false;
  }
  return waitConnectedState(true, shared);
}

bool runSingleCsWorkflow(VprBleCsWorkflowState* completed) {
  VprBleCsWorkflowState started{};
  if (!g_service.configureBleCsLink(true, kConnHandle, nullptr)) {
    return false;
  }
  if (!g_service.configureBleCsWorkflow(kConfigId, true, true, true, true, true,
                                        kMaxProcedureCount, &started)) {
    return false;
  }
  if (!started.running || !started.enabled || !started.configured ||
      !started.linkRunnable || !started.linkBound) {
    return false;
  }
  if (!g_service.waitBleCsWorkflowCompleted(kMaxProcedureCount, completed,
                                            kTimeoutMs)) {
    return false;
  }
  return completed->completed &&
         completed->completedProcedureCount == kMaxProcedureCount &&
         completed->completedConfigId == kConfigId;
}

void printSummary(const PowerProbeSummary& summary) {
  Serial.print(F("power_probe ok="));
  Serial.print(summary.ok ? 1 : 0);
  Serial.print(F(" svc="));
  Serial.print(summary.serviceVersionMajor);
  Serial.print('.');
  Serial.print(summary.serviceVersionMinor);
  Serial.print(F(" opmask=0x"));
  Serial.print(summary.opMask, HEX);
  Serial.print(F(" phase_ms="));
  Serial.print(summary.phaseServiceIdleMs);
  Serial.print('/');
  Serial.print(summary.phaseConnectedMs);
  Serial.print('/');
  Serial.print(summary.phaseCsMs);
  Serial.print('/');
  Serial.print(summary.phaseFinalIdleMs);
  Serial.print(F(" total_ms="));
  Serial.print(summary.totalRunMs);
  Serial.print(F(" cs_gap_ms="));
  Serial.print(kCsInterRunDelayMs);
  Serial.print(F(" cs_runs="));
  Serial.print(summary.csRunCount);
  Serial.print(F(" proc="));
  Serial.print(summary.completedProcedureCount);
  Serial.print(F(" last_q4="));
  Serial.print(summary.lastNominalDistanceQ4);
  Serial.print(F(" last_nominal_dist_m="));
  Serial.print(summary.lastNominalDistanceQ4 / 10000.0f, 4);
  Serial.print(F(" last_sub="));
  Serial.print(summary.lastLocalSubeventCount);
  Serial.print('/');
  Serial.print(summary.lastPeerSubeventCount);
  Serial.print(F(" last_steps="));
  Serial.print(summary.lastLocalStepCount);
  Serial.print('/');
  Serial.print(summary.lastPeerStepCount);
  Serial.print(F(" final="));
  Serial.print(summary.finalConnected ? 1 : 0);
  Serial.print('/');
  Serial.print(summary.finalLinkBound ? 1 : 0);
  Serial.print('/');
  Serial.print(summary.finalLinkRunnable ? 1 : 0);
  Serial.print('/');
  Serial.print(summary.finalWorkflowConfigured ? 1 : 0);
  Serial.print('/');
  Serial.print(summary.finalWorkflowEnabled ? 1 : 0);
  Serial.print(F("#"));
  Serial.println(summary.finalDisconnectReason, HEX);
}

PowerProbeSummary runPowerProbe() {
  PowerProbeSummary summary{};
  const uint32_t totalStartMs = millis();
  VprControllerServiceCapabilities caps{};

  if (kEnableMarkerPin) {
    pinMode(kMarkerPin, OUTPUT);
    digitalWrite(kMarkerPin, LOW);
  }

  if (!g_service.bootDefaultService(true) || !g_service.readCapabilities(&caps)) {
    return summary;
  }

  summary.serviceVersionMajor = caps.serviceVersionMajor;
  summary.serviceVersionMinor = caps.serviceVersionMinor;
  summary.opMask = caps.opMask;

  markerPulse(1U);
  const uint32_t phase0StartMs = millis();
  delay(kPhaseServiceIdleMs);
  summary.phaseServiceIdleMs = millis() - phase0StartMs;

  VprBleConnectionState configuredConnection{};
  VprBleConnectionSharedState connectedShared{};
  markerPulse(2U);
  const uint32_t phase1StartMs = millis();
  if (!configureEncryptedLink(&configuredConnection, &connectedShared)) {
    return summary;
  }
  delay(kPhaseConnectedMs);
  summary.phaseConnectedMs = millis() - phase1StartMs;

  markerPulse(3U);
  const uint32_t phase2StartMs = millis();
  while ((millis() - phase2StartMs) < kPhaseCsMs) {
    VprBleCsWorkflowState completed{};
    if (!runSingleCsWorkflow(&completed)) {
      return summary;
    }
    ++summary.csRunCount;
    summary.completedProcedureCount += completed.completedProcedureCount;
    summary.lastNominalDistanceQ4 = completed.nominalDistanceQ4;
    summary.lastLocalSubeventCount = completed.completedLocalSubeventCount;
    summary.lastPeerSubeventCount = completed.completedPeerSubeventCount;
    summary.lastLocalStepCount = completed.completedLocalStepCount;
    summary.lastPeerStepCount = completed.completedPeerStepCount;
    delay(kCsInterRunDelayMs);
  }
  summary.phaseCsMs = millis() - phase2StartMs;

  VprBleConnectionSharedState finalShared{};
  VprBleCsWorkflowState finalWorkflow{};
  markerPulse(4U);
  const uint32_t phase3StartMs = millis();
  if (!g_service.disconnectBleConnectionAndWait(kConnHandle, kDisconnectReason,
                                                &finalShared, kTimeoutMs)) {
    return summary;
  }
  delay(kPhaseFinalIdleMs);
  summary.phaseFinalIdleMs = millis() - phase3StartMs;

  if (!g_service.readBleCsWorkflowState(&finalWorkflow)) {
    return summary;
  }

  summary.finalConnected = finalShared.connected;
  summary.finalLinkBound = finalShared.csLinkBound;
  summary.finalLinkRunnable = finalShared.csLinkRunnable;
  summary.finalWorkflowConfigured = finalShared.csWorkflowConfigured;
  summary.finalWorkflowEnabled = finalShared.csWorkflowEnabled;
  summary.finalDisconnectReason = finalShared.lastDisconnectReason;
  summary.totalRunMs = millis() - totalStartMs;
  summary.ok =
      (caps.opMask & VprControllerServiceHost::kOpBleConnectionConfigure) != 0U &&
      (caps.opMask & VprControllerServiceHost::kOpBleCsLinkConfigure) != 0U &&
      (caps.opMask & VprControllerServiceHost::kOpBleCsWorkflowConfigure) != 0U &&
      summary.csRunCount > 0U &&
      summary.lastNominalDistanceQ4 > 0U &&
      !summary.finalConnected &&
      !summary.finalLinkBound &&
      !summary.finalLinkRunnable &&
      !summary.finalWorkflowConfigured &&
      !summary.finalWorkflowEnabled &&
      summary.finalDisconnectReason == kDisconnectReason &&
      !finalWorkflow.connected &&
      !finalWorkflow.linkBound &&
      !finalWorkflow.linkRunnable &&
      !finalWorkflow.configured &&
      !finalWorkflow.enabled;
  return summary;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialStartMs = millis();
  while (!Serial && (millis() - serialStartMs) < 1500U) {
  }

  g_summary = runPowerProbe();
  g_summaryReady = true;
  g_lastSummaryPrintMs = 0U;
}

void loop() {
  if (!g_summaryReady) {
    return;
  }

  const uint32_t now = millis();
  if ((now - g_lastSummaryPrintMs) < kSummaryRepeatMs) {
    return;
  }
  g_lastSummaryPrintMs = now;
  printSummary(g_summary);
}
