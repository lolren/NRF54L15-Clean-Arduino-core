#include <Arduino.h>

#define private public
#define protected public
#include <bluefruit.h>
#undef private
#undef protected

BLEDfu bledfu;
BLEDis bledis;
BLEUart bleuart;
BLEBas blebas;

volatile uint8_t g_advTypeBeforeStart = 0U;
volatile uint8_t g_advTypeAfterStart = 0U;
volatile uint8_t g_advLen = 0U;
volatile uint8_t g_scanRspLen = 0U;
volatile uint8_t g_advDirty = 0U;
volatile uint8_t g_scanRspDirty = 0U;
volatile uint8_t g_advRunning = 0U;
volatile uint16_t g_fastTimeout = 0U;
volatile uint16_t g_intervalFast = 0U;
volatile uint16_t g_intervalSlow = 0U;

static void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  g_advTypeBeforeStart = Bluefruit.Advertising.adv_type_;
  g_advLen = Bluefruit.Advertising.len_;
  g_scanRspLen = Bluefruit.ScanResponse.len_;
  g_advDirty = Bluefruit.Advertising.dirty_ ? 1U : 0U;
  g_scanRspDirty = Bluefruit.ScanResponse.dirty_ ? 1U : 0U;

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);

  g_advTypeAfterStart = Bluefruit.Advertising.adv_type_;
  g_advRunning = Bluefruit.Advertising.running_ ? 1U : 0U;
  g_fastTimeout = Bluefruit.Advertising.fast_timeout_s_;
  g_intervalFast = Bluefruit.Advertising.interval_fast_;
  g_intervalSlow = Bluefruit.Advertising.interval_slow_;
}

void setup() {
  Bluefruit.begin();
  bledfu.begin();
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();
  bleuart.begin();
  blebas.begin();
  blebas.write(100);
  startAdv();
}

void loop() {
  delay(1);
}
