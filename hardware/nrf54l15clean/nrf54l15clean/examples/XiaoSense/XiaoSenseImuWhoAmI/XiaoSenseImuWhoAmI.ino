/*
  XiaoSenseImuWhoAmI

  Sense-only probe for the onboard LSM6DS3TR-C on D12/D11.

  What it does:
  - enables the shared IMU/MIC rail with IMU_MIC_EN
  - talks to the IMU over TWIM30 on the Sense back-pad bus
  - prints the detected address and WHO_AM_I value once per second

  Expected on XIAO nRF54L15 Sense:
  - addr=0x6A
  - who=0x6A
*/

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static constexpr uint8_t kImuPrimaryAddress = 0x6AU;
static constexpr uint8_t kImuAltAddress = 0x6BU;
static constexpr uint8_t kWhoAmIRegister = 0x0FU;
static constexpr uint8_t kExpectedWhoAmI = 0x6AU;

static Twim g_imuBus(nrf54l15::TWIM30_BASE);

static bool readRegister(uint8_t address, uint8_t reg, uint8_t* value) {
  if (value == nullptr) {
    return false;
  }

  return g_imuBus.writeRead(address, &reg, 1, value, 1, 300000UL);
}

static bool probeImu(uint8_t* addressOut, uint8_t* whoAmIOut) {
  if (addressOut == nullptr || whoAmIOut == nullptr) {
    return false;
  }

  uint8_t value = 0U;
  if (readRegister(kImuPrimaryAddress, kWhoAmIRegister, &value)) {
    *addressOut = kImuPrimaryAddress;
    *whoAmIOut = value;
    return true;
  }

  if (readRegister(kImuAltAddress, kWhoAmIRegister, &value)) {
    *addressOut = kImuAltAddress;
    *whoAmIOut = value;
    return true;
  }

  return false;
}

void setup() {
  Serial.begin(115200);
  delay(250);

  BoardControl::setImuMicEnabled(true);
  delay(10);
  g_imuBus.begin(kPinImuScl, kPinImuSda, TwimFrequency::k400k);

  Serial.println("XiaoSenseImuWhoAmI");
  Serial.println("sense_bus=D12/D11 rail=IMU_MIC_EN twim=TWIM30");
  Serial.println("target=LSM6DS3TR-C");
}

void loop() {
  uint8_t address = 0U;
  uint8_t whoAmI = 0U;

  if (!probeImu(&address, &whoAmI)) {
    Serial.println("imu=not-found");
    delay(1000);
    return;
  }

  Serial.print("imu=found addr=0x");
  if (address < 0x10U) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
  Serial.print(" who=0x");
  if (whoAmI < 0x10U) {
    Serial.print('0');
  }
  Serial.print(whoAmI, HEX);
  Serial.print(" expected=0x");
  if (kExpectedWhoAmI < 0x10U) {
    Serial.print('0');
  }
  Serial.print(kExpectedWhoAmI, HEX);
  Serial.print(" match=");
  Serial.println((whoAmI == kExpectedWhoAmI) ? "yes" : "no");

  delay(1000);
}
