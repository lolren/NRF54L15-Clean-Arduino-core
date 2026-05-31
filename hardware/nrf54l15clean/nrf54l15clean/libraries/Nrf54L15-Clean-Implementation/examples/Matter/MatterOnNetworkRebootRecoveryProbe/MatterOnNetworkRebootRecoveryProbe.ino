#include <matter_onnetwork_onoff_light.h>

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core (Leader/Child/Router + UDP) before building this example."
#endif

#if !defined(NRF54L15_CLEAN_MATTER_CORE_ENABLE) || \
    (NRF54L15_CLEAN_MATTER_CORE_ENABLE == 0)
#error "Enable Tools > Matter Foundation > Experimental Compile Target before building this example."
#endif

namespace {

// This probe intentionally starts Matter without an explicit Thread dataset.
// First empty boot: it seeds the demo dataset into OpenThread settings.
// Second boot: Thread should restore from settings and Matter should report it.
constexpr bool kWipeThreadSettingsOnBoot = false;
constexpr bool kSeedDemoDatasetWhenSettingsEmpty = true;
constexpr uint32_t kStatusIntervalMs = 1000UL;

xiao_nrf54l15::Nrf54MatterOnNetworkOnOffLightNode gNode;
xiao_nrf54l15::Nrf54ThreadExperimental::Role gLastRole =
    xiao_nrf54l15::Nrf54ThreadExperimental::Role::kUnknown;
uint32_t gLastStatusMs = 0U;
bool gSeedAttempted = false;
bool gDatasetHexPrinted = false;
char gDatasetHex[(OT_OPERATIONAL_DATASET_MAX_LENGTH * 2U) + 1U] = {0};

void printRestoreDiagnostics(
    const xiao_nrf54l15::Nrf54ThreadExperimental::DatasetRestoreDiagnostics&
        diagnostics) {
  Serial.print(" restore_attempted=");
  Serial.print(diagnostics.attempted ? 1 : 0);
  Serial.print(" restore_restored=");
  Serial.print(diagnostics.restored ? 1 : 0);
  Serial.print(" restore_source=");
  Serial.print(diagnostics.sourceName);
  Serial.print(" restore_tlv_len=");
  Serial.print(diagnostics.restoredTlvLength);
  Serial.print(" restore_error=");
  Serial.print(static_cast<int>(diagnostics.error));
  Serial.print(" restore_blocker=");
  Serial.print(diagnostics.blockerName);
}

void printStatus(const char* reason) {
  xiao_nrf54l15::MatterOnNetworkOnOffLightStatus status;
  if (!gNode.snapshot(&status)) {
    Serial.print("matter_reboot reason=");
    Serial.print(reason);
    Serial.println(" snapshot=0");
    return;
  }

  Serial.print("matter_reboot reason=");
  Serial.print(reason);
  Serial.print(" ready=");
  Serial.print(status.readyForOnNetworkCommissioning ? 1 : 0);
  Serial.print(" role=");
  Serial.print(gNode.thread().roleName());
  Serial.print(" attached=");
  Serial.print(status.threadAttached ? 1 : 0);
  Serial.print(" dataset_source=");
  Serial.print(xiao_nrf54l15::Nrf54MatterOnNetworkOnOffLightNode::
                   datasetSourceName(status.datasetSource));
  Serial.print(" readiness=");
  Serial.print(status.readinessSummary.phaseName);
  Serial.print("/");
  Serial.print(status.readinessSummary.blockerName);
  printRestoreDiagnostics(status.threadRestoreDiagnostics);
  Serial.println();
}

void maybeSeedDemoDataset() {
  if (!kSeedDemoDatasetWhenSettingsEmpty || gSeedAttempted) {
    return;
  }

  xiao_nrf54l15::MatterOnNetworkOnOffLightStatus status;
  if (!gNode.snapshot(&status) ||
      status.threadRestoreDiagnostics.restored ||
      (!status.threadRestoreDiagnostics.wipeRequested &&
       (!status.threadRestoreDiagnostics.attempted ||
        status.threadRestoreDiagnostics.error != OT_ERROR_NOT_FOUND))) {
    return;
  }

  gSeedAttempted = true;
  const bool datasetOk = gNode.useDemoThreadDataset();
  const bool restartOk = datasetOk && gNode.thread().restart(false);
  Serial.print("matter_reboot seed_demo_dataset=");
  Serial.print(datasetOk ? 1 : 0);
  Serial.print(" thread_restart=");
  Serial.println(restartOk ? 1 : 0);
  Serial.println("matter_reboot note=reset_after_attach_then_expect_restore_restored_1");
}

void maybePrintDatasetHex() {
  if (gDatasetHexPrinted) {
    return;
  }

  if (!gNode.exportOpenThreadDatasetHex(gDatasetHex, sizeof(gDatasetHex),
                                        nullptr)) {
    return;
  }

  Serial.print("matter_reboot dataset_hex=");
  Serial.println(gDatasetHex);
  gDatasetHexPrinted = true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

  xiao_nrf54l15::MatterOnNetworkOnOffLightConfig config;
  config.storageNamespace = "matter_rb_probe";
  config.lightStorageNamespace = "matter_rb_light";
  config.restorePersistentState = true;
  config.wipeThreadSettings = kWipeThreadSettingsOnBoot;
  config.autoStartThread = true;
  config.useDemoDataset = false;

  const bool beginOk = gNode.begin(&config);
  Serial.print("matter_reboot begin=");
  Serial.println(beginOk ? 1 : 0);
  Serial.print("matter_reboot wipe_on_boot=");
  Serial.println(kWipeThreadSettingsOnBoot ? 1 : 0);
  Serial.println("matter_reboot first_empty_boot_seeds_demo_dataset");
  Serial.println("matter_reboot second_boot_should_show_restore_restored_1");
  printStatus("boot");
}

void loop() {
  gNode.process();
  maybeSeedDemoDataset();
  maybePrintDatasetHex();

  const xiao_nrf54l15::Nrf54ThreadExperimental::Role currentRole =
      gNode.thread().role();
  const uint32_t nowMs = millis();
  const bool roleChanged = (currentRole != gLastRole);
  if (roleChanged || (nowMs - gLastStatusMs) >= kStatusIntervalMs) {
    gLastRole = currentRole;
    gLastStatusMs = nowMs;
    printStatus(roleChanged ? "role" : "status");
  }
}
