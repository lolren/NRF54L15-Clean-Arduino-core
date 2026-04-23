/*
  XiaoSenseImuAccelGyro

  Sense-only streaming example for the onboard LSM6DS3TR-C on D12/D11.

  What it does:
  - enables the shared IMU/MIC rail with IMU_MIC_EN
  - configures the IMU for 104 Hz accel (+-2 g) and gyro (245 dps)
  - prints accel in mg and gyro in mdps

  Usage:
  - open Serial Monitor at 115200
  - move or rotate the board and watch the values change
*/

#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static constexpr uint8_t kImuAddress = 0x6AU;
static constexpr uint8_t kWhoAmIRegister = 0x0FU;
static constexpr uint8_t kCtrl1XlRegister = 0x10U;
static constexpr uint8_t kCtrl2GRegister = 0x11U;
static constexpr uint8_t kCtrl3CRegister = 0x12U;
static constexpr uint8_t kStatusRegister = 0x1EU;
static constexpr uint8_t kGyroStartRegister = 0x22U;
static constexpr uint8_t kExpectedWhoAmI = 0x6AU;
static constexpr uint8_t kAccelDataReadyMask = 0x01U;
static constexpr uint8_t kGyroDataReadyMask = 0x02U;

static Twim g_imuBus(nrf54l15::TWIM30_BASE);

static bool writeRegister(uint8_t reg, uint8_t value) {
  const uint8_t payload[2] = {reg, value};
  return g_imuBus.write(kImuAddress, payload, sizeof(payload), 300000UL);
}

static bool readRegister(uint8_t reg, uint8_t* value) {
  if (value == nullptr) {
    return false;
  }

  return g_imuBus.writeRead(kImuAddress, &reg, 1, value, 1, 300000UL);
}

static bool readBurst(uint8_t reg, uint8_t* data, size_t len) {
  if (data == nullptr || len == 0U) {
    return false;
  }

  return g_imuBus.writeRead(kImuAddress, &reg, 1, data, len, 400000UL);
}

static int16_t readLeI16(const uint8_t* data) {
  return static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) |
                              static_cast<uint16_t>(data[0]));
}

static bool configureImu(uint8_t* whoAmIOut) {
  if (whoAmIOut == nullptr) {
    return false;
  }

  uint8_t whoAmI = 0U;
  if (!readRegister(kWhoAmIRegister, &whoAmI)) {
    return false;
  }
  *whoAmIOut = whoAmI;

  // BDU=1, IF_INC=1 so multi-byte reads stay coherent and auto-increment.
  if (!writeRegister(kCtrl3CRegister, 0x44U)) {
    return false;
  }

  // 104 Hz accel, +-2 g full-scale.
  if (!writeRegister(kCtrl1XlRegister, 0x40U)) {
    return false;
  }

  // 104 Hz gyro, 245 dps full-scale.
  return writeRegister(kCtrl2GRegister, 0x40U);
}

void setup() {
  Serial.begin(115200);
  delay(250);

  BoardControl::setImuMicEnabled(true);
  delay(10);
  g_imuBus.begin(kPinImuScl, kPinImuSda, TwimFrequency::k400k);

  uint8_t whoAmI = 0U;
  const bool ok = configureImu(&whoAmI);

  Serial.println("XiaoSenseImuAccelGyro");
  Serial.println("target=LSM6DS3TR-C");
  Serial.print("imu_init=");
  Serial.print(ok ? "ok" : "fail");
  Serial.print(" who=0x");
  if (whoAmI < 0x10U) {
    Serial.print('0');
  }
  Serial.print(whoAmI, HEX);
  Serial.print(" expected=0x");
  if (kExpectedWhoAmI < 0x10U) {
    Serial.print('0');
  }
  Serial.println(kExpectedWhoAmI, HEX);
}

void loop() {
  uint8_t status = 0U;
  if (!readRegister(kStatusRegister, &status)) {
    Serial.println("imu=status-read-failed");
    delay(250);
    return;
  }

  if ((status & (kAccelDataReadyMask | kGyroDataReadyMask)) == 0U) {
    delay(20);
    return;
  }

  uint8_t raw[12] = {0};
  if (!readBurst(kGyroStartRegister, raw, sizeof(raw))) {
    Serial.println("imu=burst-read-failed");
    delay(250);
    return;
  }

  const int16_t gx = readLeI16(&raw[0]);
  const int16_t gy = readLeI16(&raw[2]);
  const int16_t gz = readLeI16(&raw[4]);
  const int16_t ax = readLeI16(&raw[6]);
  const int16_t ay = readLeI16(&raw[8]);
  const int16_t az = readLeI16(&raw[10]);

  const long gxMdps = static_cast<long>(gx) * 875L / 100L;
  const long gyMdps = static_cast<long>(gy) * 875L / 100L;
  const long gzMdps = static_cast<long>(gz) * 875L / 100L;
  const long axMg = static_cast<long>(ax) * 61L / 1000L;
  const long ayMg = static_cast<long>(ay) * 61L / 1000L;
  const long azMg = static_cast<long>(az) * 61L / 1000L;

  Serial.print("accel_mg=");
  Serial.print(axMg);
  Serial.print(',');
  Serial.print(ayMg);
  Serial.print(',');
  Serial.print(azMg);
  Serial.print(" gyro_mdps=");
  Serial.print(gxMdps);
  Serial.print(',');
  Serial.print(gyMdps);
  Serial.print(',');
  Serial.println(gzMdps);

  delay(100);
}
