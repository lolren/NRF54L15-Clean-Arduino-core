#include <Arduino.h>

#include <stdio.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

/*
 * BleAdvertiser
 *
 * General-purpose legacy BLE advertiser. More explicit than minimal examples:
 *   - Raw advertising payload (Flags + Complete Name + Manufacturer Data).
 *   - Periodic Serial logging of event count.
 *   - Low-power latency mode between events.
 *
 * This sketch uses the per-event enable/collapse RF pattern which is required
 * for phone discoverability on XIAO nRF54L15. The RF switch is powered only
 * during each advertiseEvent() call and collapsed during the idle delay.
 *
 * Note: setDeviceAddress() is intentionally omitted. Validation on this HAL
 * showed that calling setDeviceAddress() before advertiseEvent() causes the
 * device to not be discoverable by phones/scanners. Use the default
 * FICR-derived address for reliable discoverability.
 *
 * To receive and decode this sketch's packets, use BlePassiveScanner or
 * BleActiveScanner on a second board.
 *
 * Tip: the raw kAdvPayload layout is intentionally visible here so you can
 * learn the AD-structure format. Each field is: [length, type, data...].
 */

static BleRadio g_ble;
static PowerManager g_power;

static uint32_t g_lastLogMs = 0;
static uint32_t g_advEvents = 0;

// kTxPowerDbm: 0 dBm for reliable phone discoverability.
static constexpr int8_t kTxPowerDbm = 0;
// kAdvertisingIntervalMs: delay() between advertising events (milliseconds).
static constexpr uint32_t kAdvertisingIntervalMs = 100UL;
// kInterChannelDelayUs: pause between ch37/38/39 transmissions (microseconds).
static constexpr uint32_t kInterChannelDelayUs = 350U;
// kAdvertisingSpinLimit: max time to wait for the radio to complete each
// channel transmission (microseconds). 700 000 us = 700 ms.
static constexpr uint32_t kAdvertisingSpinLimit = 700000UL;

void setup() {
  Serial.begin(115200);
  delay(350);

  Serial.print("\r\nBleAdvertiser start\r\n");

  BoardControl::setBatterySenseEnabled(false);
  BoardControl::setImuMicEnabled(false);
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);

  // The name is carried inside the raw advertising payload, not by
  // setAdvertisingName(...), so the payload layout is fully visible here.
  static const uint8_t kAdvPayload[] = {
      2, 0x01, 0x06,                                 // Flags
      12, 0x09, 'X', 'I', 'A', 'O', '-', '5', '4',  // Complete name
      '-', 'C', 'L', 'N',
      5, 0xFF, 0x34, 0x12, 0x54, 0x15                // MFG data
  };

  // Enable RF path for init, then collapse. The loop re-enables it around
  // each advertiseEvent() call — required for phone discoverability.
  BoardControl::enableRfPath(BoardAntennaPath::kCeramic);
  bool ok = g_ble.begin(kTxPowerDbm);
  if (ok) {
    // Set after begin() so the radio subsystem is already configured.
    g_power.setLatencyMode(PowerLatencyMode::kLowPower);
  }
  if (ok) {
    // advertiseEvent() is TX-only on this HAL, so use a non-connectable PDU.
    ok = g_ble.setAdvertisingPduType(BleAdvPduType::kAdvNonConnInd);
  }
  if (ok) {
    ok = g_ble.setAdvertisingData(kAdvPayload, sizeof(kAdvPayload));
  }
  BoardControl::collapseRfPathIdle();

  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
}

void loop() {
  // Per-event enable/collapse required for phone discoverability on XIAO nRF54L15.
  BoardControl::enableRfPath(BoardAntennaPath::kCeramic);
  const bool txOk = g_ble.advertiseEvent(kInterChannelDelayUs, kAdvertisingSpinLimit);
  BoardControl::collapseRfPathIdle();

  ++g_advEvents;

  // User LED is active-low on XIAO.
  Gpio::write(kPinUserLed, (g_advEvents & 0x1U) == 0U);

  const uint32_t now = millis();
  if ((now - g_lastLogMs) >= 1000UL) {
    g_lastLogMs = now;

    char line[96];
    snprintf(line, sizeof(line),
             "t=%lu adv_events=%lu last=%s\r\n",
             static_cast<unsigned long>(now),
             static_cast<unsigned long>(g_advEvents),
             txOk ? "OK" : "FAIL");
    Serial.print(line);
  }

  delay(kAdvertisingIntervalMs);
}
