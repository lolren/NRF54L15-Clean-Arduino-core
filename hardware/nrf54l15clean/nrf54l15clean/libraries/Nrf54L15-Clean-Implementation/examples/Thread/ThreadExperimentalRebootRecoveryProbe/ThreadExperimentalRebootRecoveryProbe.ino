#include <nrf54_thread_experimental.h>

using xiao_nrf54l15::Nrf54ThreadExperimental;

#if !defined(NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE) || \
    (NRF54L15_CLEAN_OPENTHREAD_CORE_ENABLE == 0)
#error "Enable Tools > Thread Core > Experimental Stage Core (Leader/Child/Router + UDP) before building this example."
#endif

namespace {

// Set this to true for one upload if you want to force a clean first pass.
// Set it back to false before checking restore-after-reset behavior.
constexpr bool kWipeThreadSettingsOnBoot = false;
constexpr bool kSeedDemoDatasetWhenSettingsEmpty = true;
constexpr uint32_t kStatusIntervalMs = 1000UL;

Nrf54ThreadExperimental gThread;
Nrf54ThreadExperimental::Role gLastRole =
    Nrf54ThreadExperimental::Role::kUnknown;
uint32_t gLastStatusMs = 0U;
bool gSeedAttempted = false;
bool gDatasetHexPrinted = false;
char gDatasetHex[(OT_OPERATIONAL_DATASET_MAX_LENGTH * 2U) + 1U] = {0};

void printRestoreDiagnostics(
    const Nrf54ThreadExperimental::DatasetRestoreDiagnostics& diagnostics) {
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
  Nrf54ThreadExperimental::DatasetRestoreDiagnostics diagnostics;
  (void)gThread.getDatasetRestoreDiagnostics(&diagnostics);

  Serial.print("thread_reboot reason=");
  Serial.print(reason);
  Serial.print(" role=");
  Serial.print(gThread.roleName());
  Serial.print(" rloc16=0x");
  Serial.print(gThread.rloc16(), HEX);
  Serial.print(" attached=");
  Serial.print(gThread.attached() ? 1 : 0);
  Serial.print(" dataset=");
  Serial.print(gThread.datasetConfigured() ? 1 : 0);
  Serial.print(" err=");
  Serial.print(static_cast<int>(gThread.lastError()));
  printRestoreDiagnostics(diagnostics);
  Serial.println();
}

void maybeSeedDemoDataset() {
  if (!kSeedDemoDatasetWhenSettingsEmpty || gSeedAttempted ||
      gThread.datasetConfigured()) {
    return;
  }

  Nrf54ThreadExperimental::DatasetRestoreDiagnostics diagnostics;
  if (!gThread.getDatasetRestoreDiagnostics(&diagnostics) ||
      diagnostics.restored ||
      (!diagnostics.wipeRequested &&
       (!diagnostics.attempted || diagnostics.error != OT_ERROR_NOT_FOUND))) {
    return;
  }

  otOperationalDataset dataset = {};
  Nrf54ThreadExperimental::buildDemoDataset(&dataset);
  gSeedAttempted = true;
  const bool ok = gThread.setActiveDataset(dataset);
  Serial.print("thread_reboot seed_demo_dataset=");
  Serial.println(ok ? 1 : 0);
  Serial.println("thread_reboot note=reset_after_attach_then_expect_restore_restored_1");
}

void maybePrintDatasetHex() {
  if (gDatasetHexPrinted) {
    return;
  }

  if (!gThread.exportConfiguredOrActiveDatasetHex(gDatasetHex,
                                                  sizeof(gDatasetHex),
                                                  nullptr)) {
    return;
  }

  Serial.print("thread_reboot dataset_hex=");
  Serial.println(gDatasetHex);
  gDatasetHexPrinted = true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && (millis() - waitStart) < 1500UL) {
  }

  const bool beginOk = gThread.begin(kWipeThreadSettingsOnBoot);
  Serial.print("thread_reboot begin=");
  Serial.println(beginOk ? 1 : 0);
  Serial.print("thread_reboot wipe_on_boot=");
  Serial.println(kWipeThreadSettingsOnBoot ? 1 : 0);
  Serial.println("thread_reboot first_empty_boot_seeds_demo_dataset");
  Serial.println("thread_reboot second_boot_should_show_restore_restored_1");
  printStatus("boot");
}

void loop() {
  gThread.process();
  maybeSeedDemoDataset();
  maybePrintDatasetHex();

  const Nrf54ThreadExperimental::Role currentRole = gThread.role();
  const uint32_t nowMs = millis();
  const bool roleChanged = (currentRole != gLastRole);
  if (roleChanged || (nowMs - gLastStatusMs) >= kStatusIntervalMs) {
    gLastRole = currentRole;
    gLastStatusMs = nowMs;
    printStatus(roleChanged ? "role" : "status");
  }
}
