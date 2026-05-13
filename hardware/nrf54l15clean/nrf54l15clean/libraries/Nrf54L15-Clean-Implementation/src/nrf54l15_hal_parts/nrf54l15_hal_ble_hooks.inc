extern "C" void nrf54l15_clean_ble_idle_service(void) {
  // Serial.print()/delay() from BLE callbacks eventually call yield(), which
  // re-enters this service path. Without a guard the central callback stack can
  // recursively consume more connection events and corrupt user-visible
  // behavior.
  // However, background connection service is safe to run during re-entry
  // because it uses its own connectionServiceBusy_ guard. This lets sketches
  // that call delay() inside connect callbacks still service BLE events.
  if (g_activeBleRadio != nullptr) {
    if (g_activeBleRadio->isBackgroundConnectionServiceEnabled()) {
      g_activeBleRadio->serviceBackgroundConnection(0U);
    } else {
      g_activeBleRadio->serviceBackgroundConnection();
    }
  }

  if (g_bleIdleServiceActive) {
    return;
  }
  g_bleIdleServiceActive = true;

  if (g_activeBleRadio != nullptr) {
    g_activeBleRadio->serviceDeferredApplicationWork();
  }
  if (g_bleBackgroundRadio != nullptr && g_bleBackgroundRadio != g_activeBleRadio) {
    g_bleBackgroundRadio->serviceDeferredApplicationWork();
  }
  if (nrf54l15_bluefruit_compat_idle_service != nullptr) {
    nrf54l15_bluefruit_compat_idle_service();
  }

  g_bleIdleServiceActive = false;
}

extern "C" void nrf54l15_clean_ble_yield_service(void) {
  // Same re-entry relaxation as idle_service: background connection polling
  // is safe to run during callbacks thanks to connectionServiceBusy_.
  if (g_activeBleRadio != nullptr) {
    g_activeBleRadio->serviceBackgroundConnection(0U);
  }

  if (g_bleIdleServiceActive) {
    return;
  }
  g_bleIdleServiceActive = true;

  if (g_activeBleRadio != nullptr) {
    g_activeBleRadio->serviceDeferredApplicationWork();
  }
  if (g_bleBackgroundRadio != nullptr && g_bleBackgroundRadio != g_activeBleRadio) {
    g_bleBackgroundRadio->serviceBackgroundConnection(0U);
    g_bleBackgroundRadio->serviceDeferredApplicationWork();
  }
  if (nrf54l15_bluefruit_compat_yield_service != nullptr) {
    nrf54l15_bluefruit_compat_yield_service();
  }

  g_bleIdleServiceActive = false;
}

extern "C" uint32_t nrf54l15_clean_ble_idle_sleep_cap_us(void) {
  uint32_t capUs = 0U;

  if (g_activeBleRadio != nullptr && g_activeBleRadio->isConnected() &&
      !g_activeBleRadio->isBackgroundConnectionServiceEnabled()) {
    capUs = 1000U;
  }
  if (nrf54l15_bluefruit_compat_idle_sleep_cap_us != nullptr) {
    const uint32_t compatCapUs = nrf54l15_bluefruit_compat_idle_sleep_cap_us();
    if (compatCapUs != 0U && (capUs == 0U || compatCapUs < capUs)) {
      capUs = compatCapUs;
    }
  }

  return capUs;
}

extern "C" uint32_t nrf54l15_ble_grtc_reserved_cc_mask(void) {
  uint32_t mask = 0U;
  mask |= (1UL << kBleBackgroundCompareChannel);
  mask |= (1UL << kBleBackgroundAdvPrewarmCompareChannel);
  mask |= (1UL << kBleBackgroundAdvTxCompareChannel);
  mask |= (1UL << kBleBackgroundConnPrewarmCompareChannel);
  mask |= (1UL << kBleBackgroundAdvCleanupCompareChannel);
  mask |= (1UL << kBleBackgroundAdvStage1ServiceCompareChannel);
  mask |= (1UL << kBleBackgroundAdvStage1TxCompareChannel);
  mask |= (1UL << kBleBackgroundAdvStage2ServiceCompareChannel);
  mask |= (1UL << kBleBackgroundAdvStage2TxCompareChannel);
  mask |= (1UL << kBleBackgroundAdvFinalCleanupCompareChannel);
  mask |= (1UL << kBleBackgroundConnRxCompareChannel);
  return mask;
}
