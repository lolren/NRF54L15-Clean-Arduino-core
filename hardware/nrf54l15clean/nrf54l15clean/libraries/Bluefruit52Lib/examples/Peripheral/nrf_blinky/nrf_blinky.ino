#include <bluefruit.h>

static constexpr uint8_t LBS_UUID_SERVICE[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00,
};
static constexpr uint8_t LBS_UUID_CHR_BUTTON[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00,
};
static constexpr uint8_t LBS_UUID_CHR_LED[16] = {
    0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
    0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00,
};

BLEService lbs(LBS_UUID_SERVICE);
BLECharacteristic lsbButton(LBS_UUID_CHR_BUTTON);
BLECharacteristic lsbLED(LBS_UUID_CHR_LED);

static const uint8_t kButtonPin = PIN_D0;

static uint8_t g_buttonState = 0U;

void setLed(bool on) {
  digitalWrite(LED_BUILTIN, on ? LOW : HIGH);
}

void ledWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  (void)conn_hdl;
  (void)chr;
  if (len > 0U) {
    setLed(data[0] != 0U);
  }
}

void startAdv() {
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(lbs);
  // Keep a shortened name in ADV so passive scanners still show a label.
  Bluefruit.Advertising.addName();
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void connectCallback(uint16_t conn_handle) {
  (void)conn_handle;
  setLed(true);
  lsbLED.write8(1U);
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;
  setLed(false);
  lsbLED.write8(0U);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setLed(false);
  pinMode(kButtonPin, INPUT_PULLUP);
  g_buttonState = (digitalRead(kButtonPin) == LOW) ? 1U : 0U;

  Serial.begin(115200);

  Bluefruit.begin(1, 0);
  Bluefruit.setTxPower(0);
  Bluefruit.setName("XIAO54 Blinky");
  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  lbs.begin();

  lsbButton.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  lsbButton.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  lsbButton.setFixedLen(1);
  lsbButton.begin();
  lsbButton.write8(g_buttonState);

  lsbLED.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  lsbLED.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  lsbLED.setFixedLen(1);
  lsbLED.setWriteCallback(ledWriteCallback);
  lsbLED.begin();
  lsbLED.write8(0U);

  startAdv();

  Serial.println("Bluefruit-style nRF Blinky");
}

void loop() {
  delay(10);

  const uint8_t newState = (digitalRead(kButtonPin) == LOW) ? 1U : 0U;
  if (newState == g_buttonState) {
    return;
  }

  g_buttonState = newState;
  lsbButton.write8(g_buttonState);
  if (Bluefruit.connected() && lsbButton.notifyEnabled()) {
    lsbButton.notify8(g_buttonState);
  }
}
