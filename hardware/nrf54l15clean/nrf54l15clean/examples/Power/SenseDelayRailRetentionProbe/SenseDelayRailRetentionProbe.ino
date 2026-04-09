/*
  SenseDelayRailRetentionProbe

  Functional validation sketch for issue #43 on XIAO nRF54L15 Sense.

  Goal:
  - prove that plain delay() in `Low Power (WFI Idle)` does not silently
    collapse the XIAO board-control rails mid-sleep
  - sample IMU_MIC_EN / RF_SW / VBAT_EN halfway through delay() from a GRTC IRQ
  - immediately probe the Sense IMU and VBAT divider after wake

  Expected on a fixed core:
  - mid-delay pin samples stay HIGH
  - immediate post-delay WHO_AM_I reads keep succeeding on Sense boards
  - VBAT raw stays readable with VBAT_EN held HIGH

  Recommended board menu:
  - Power Profile: Low Power (WFI Idle)
  - BLE Support: Disabled
*/

#include <Arduino.h>
#include <Wire.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static Grtc g_grtc;

static constexpr uint8_t kProbeCompareChannel = 6U;
static constexpr uint32_t kMidSampleDelayUs = 250000UL;
static constexpr uint8_t kImuWhoAmIReg = 0x0FU;
static const uint8_t kImuAddresses[] = {0x6AU, 0x6BU};

static volatile uint32_t g_midSampleCount = 0UL;
static volatile uint8_t g_midImuPin = 0U;
static volatile uint8_t g_midRfPin = 0U;
static volatile uint8_t g_midRfCtlPin = 0U;
static volatile uint8_t g_midVbatPin = 0U;
static volatile uint8_t g_midSampleArmed = 0U;

static bool g_haveImu = false;
static uint8_t g_imuAddress = 0U;

static bool readWhoAmI(uint8_t address, uint8_t* whoAmI) {
  if (whoAmI == nullptr) {
    return false;
  }

  Wire1.beginTransmission(address);
  Wire1.write(kImuWhoAmIReg);
  if (Wire1.endTransmission(false) != 0U) {
    return false;
  }

  const int received = Wire1.requestFrom(static_cast<int>(address), 1, 1);
  if (received != 1 || Wire1.available() <= 0) {
    return false;
  }

  *whoAmI = static_cast<uint8_t>(Wire1.read());
  return true;
}

static bool detectSenseImu(uint8_t* address, uint8_t* whoAmI) {
  if (address == nullptr || whoAmI == nullptr) {
    return false;
  }

  for (size_t i = 0U; i < (sizeof(kImuAddresses) / sizeof(kImuAddresses[0])); ++i) {
    uint8_t id = 0U;
    if (readWhoAmI(kImuAddresses[i], &id)) {
      *address = kImuAddresses[i];
      *whoAmI = id;
      return true;
    }
  }

  return false;
}

static void armMidDelaySample() {
  g_midSampleArmed = 1U;
  (void)g_grtc.clearCompareEvent(kProbeCompareChannel);
  g_grtc.enableCompareInterrupt(kProbeCompareChannel, true);
  (void)g_grtc.setCompareAbsoluteUs(kProbeCompareChannel,
                                    g_grtc.counter() +
                                        static_cast<uint64_t>(kMidSampleDelayUs),
                                    true);
}

extern "C" void nrf54l15_ble_grtc_irq_service(void) {
  if (NRF_GRTC->EVENTS_COMPARE[kProbeCompareChannel] == 0U) {
    return;
  }

  NRF_GRTC->EVENTS_COMPARE[kProbeCompareChannel] = 0U;
  NRF_GRTC->CC[kProbeCompareChannel].CCEN =
      (GRTC_CC_CCEN_ACTIVE_Disable << GRTC_CC_CCEN_ACTIVE_Pos);
  NRF54L15_GRTC_INTENCLR_REG(NRF_GRTC) = (1UL << kProbeCompareChannel);

  g_midImuPin = static_cast<uint8_t>(digitalRead(IMU_MIC_EN) != 0 ? 1U : 0U);
  g_midRfPin = static_cast<uint8_t>(digitalRead(RF_SW) != 0 ? 1U : 0U);
  g_midRfCtlPin = static_cast<uint8_t>(digitalRead(RF_SW_CTL) != 0 ? 1U : 0U);
  g_midVbatPin = static_cast<uint8_t>(digitalRead(VBAT_EN) != 0 ? 1U : 0U);
  g_midSampleArmed = 0U;
  ++g_midSampleCount;
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  pinMode(IMU_MIC_EN, OUTPUT);
  pinMode(RF_SW, OUTPUT);
  pinMode(RF_SW_CTL, OUTPUT);
  pinMode(VBAT_EN, OUTPUT);

  digitalWrite(IMU_MIC_EN, HIGH);
  digitalWrite(RF_SW, HIGH);
  digitalWrite(RF_SW_CTL, LOW);
  digitalWrite(VBAT_EN, HIGH);

  Wire1.begin();
  Wire1.setClock(400000UL);

  // Force the low-power delay timebase/IRQ path up once before the first
  // actual probe loop, then configure our extra compare channel on the same
  // GRTC instance.
  delay(1);
  (void)g_grtc.begin(GrtcClockSource::kLfxo);
  g_grtc.enableCompareInterrupt(kProbeCompareChannel, true);

  uint8_t whoAmI = 0U;
  g_haveImu = detectSenseImu(&g_imuAddress, &whoAmI);

  Serial.println("SenseDelayRailRetentionProbe");
  Serial.println("Holding IMU_MIC_EN/RF_SW/VBAT_EN HIGH across plain delay().");
  Serial.print("sense_imu=");
  Serial.print(g_haveImu ? "yes" : "no");
  if (g_haveImu) {
    Serial.print(" addr=0x");
    if (g_imuAddress < 0x10U) {
      Serial.print('0');
    }
    Serial.print(g_imuAddress, HEX);
    Serial.print(" who=0x");
    if (whoAmI < 0x10U) {
      Serial.print('0');
    }
    Serial.println(whoAmI, HEX);
  } else {
    Serial.println(" (expected on non-Sense boards)");
  }
}

void loop() {
  armMidDelaySample();
  delay(500);

  uint8_t whoAmI = 0U;
  const bool whoOk = g_haveImu && readWhoAmI(g_imuAddress, &whoAmI);
  const int raw = analogRead(VBAT_READ);

  Serial.print("mid_count=");
  Serial.print(g_midSampleCount);
  Serial.print(" armed=");
  Serial.print(g_midSampleArmed);
  Serial.print(" mid_imu=");
  Serial.print(g_midImuPin);
  Serial.print(" mid_rf=");
  Serial.print(g_midRfPin);
  Serial.print(" mid_rfctl=");
  Serial.print(g_midRfCtlPin);
  Serial.print(" mid_vbat=");
  Serial.print(g_midVbatPin);
  Serial.print(" post_imu=");
  Serial.print(digitalRead(IMU_MIC_EN));
  Serial.print(" post_rf=");
  Serial.print(digitalRead(RF_SW));
  Serial.print(" post_rfctl=");
  Serial.print(digitalRead(RF_SW_CTL));
  Serial.print(" post_vbat=");
  Serial.print(digitalRead(VBAT_EN));
  Serial.print(" vbat_raw=");
  Serial.print(raw);
  Serial.print(" imu_ok=");
  Serial.print(whoOk ? 1 : 0);
  if (whoOk) {
    Serial.print(" who=0x");
    if (whoAmI < 0x10U) {
      Serial.print('0');
    }
    Serial.print(whoAmI, HEX);
  }
  Serial.println();

  delay(1000);
}
