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

extern "C" void nrf54l15_clean_ble_application_work_service(void) {
  if (g_bleIdleServiceActive) {
    return;
  }
  g_bleIdleServiceActive = true;

  // Keep the lightweight helper responsible for arming or running the
  // background connection scheduler. Without this step, sketches that enable
  // background connection service immediately after CONNECT_IND can miss the
  // first data event on slower or unlucky handshakes and become "connected"
  // only long enough to stop advertising.
  if (g_activeBleRadio != nullptr) {
    g_activeBleRadio->serviceBackgroundConnection(0U);
  }
  if (g_bleBackgroundRadio != nullptr &&
      g_bleBackgroundRadio != g_activeBleRadio) {
    g_bleBackgroundRadio->serviceBackgroundConnection(0U);
  }

  // NUS-style foreground service loops need deferred GATT peer-write
  // callbacks to run promptly so RX bytes reach sketch-owned ring buffers.
  // Do not route this helper through the broader deferred-application policy:
  // that path intentionally postpones noncritical work when the next
  // connection event is close, which is reasonable for trace printing but can
  // indefinitely starve RX callbacks on fast centrals.
  if (g_activeBleRadio != nullptr) {
    g_activeBleRadio->dispatchDeferredGattWrites();
  }
  if (g_bleBackgroundRadio != nullptr &&
      g_bleBackgroundRadio != g_activeBleRadio) {
    g_bleBackgroundRadio->dispatchDeferredGattWrites();
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

extern "C" uint8_t nrf54l15_clean_low_power_micro_delay_sleep_allowed(void) {
  if (g_activeBleRadio != nullptr && g_activeBleRadio->isConnected()) {
    return 1U;
  }
  if (g_bleBackgroundRadio != nullptr && g_bleBackgroundRadio->isConnected()) {
    return 1U;
  }
  return 0U;
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
