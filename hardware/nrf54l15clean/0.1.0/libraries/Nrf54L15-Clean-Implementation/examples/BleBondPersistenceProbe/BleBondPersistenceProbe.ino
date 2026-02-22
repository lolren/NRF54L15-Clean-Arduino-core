#include <Arduino.h>

#include "nrf54l15_hal.h"

using namespace xiao_nrf54l15;

static BleRadio g_ble;
static PowerManager g_power;

static bool g_prevConnected = false;
static bool g_prevEncrypted = false;
static bool g_connectionAnnounced = false;
static uint32_t g_lastAdvLogMs = 0U;
static uint32_t g_lastBondLogMs = 0U;

static void onBleTrace(const char* message, void* context) {
  (void)context;
  if (message == nullptr) {
    return;
  }
  Serial.print("[trace] ");
  Serial.print(message);
  Serial.print("\r\n");
}

static void printAddress(const uint8_t* addr) {
  if (addr == nullptr) {
    return;
  }
  for (int i = 5; i >= 0; --i) {
    if (addr[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(addr[i], HEX);
    if (i > 0) {
      Serial.print(':');
    }
  }
}

static void printBondSummary() {
  BleBondRecord bond{};
  if (!g_ble.getBondRecord(&bond)) {
    Serial.print("bond: none\r\n");
    return;
  }
  Serial.print("bond: peer=");
  printAddress(bond.peerAddress);
  Serial.print(" keySize=");
  Serial.print(bond.keySize);
  Serial.print(" ediv=0x");
  if (bond.ediv < 0x1000U) {
    Serial.print('0');
  }
  if (bond.ediv < 0x0100U) {
    Serial.print('0');
  }
  if (bond.ediv < 0x0010U) {
    Serial.print('0');
  }
  Serial.print(bond.ediv, HEX);
  Serial.print("\r\n");
}

void setup() {
  Serial.begin(115200);
  delay(350);

  Serial.print("\r\nBleBondPersistenceProbe start\r\n");

  g_power.setLatencyMode(PowerLatencyMode::kLowPower);
  Gpio::configure(kPinUserLed, GpioDirection::kOutput, GpioPull::kDisabled);
  Gpio::write(kPinUserLed, true);
  Gpio::configure(kPinUserButton, GpioDirection::kInput, GpioPull::kPullUp);
  g_ble.setTraceCallback(onBleTrace, nullptr);

  bool buttonPressed = true;
  if (!Gpio::read(kPinUserButton, &buttonPressed)) {
    buttonPressed = false;
  } else {
    buttonPressed = !buttonPressed;  // Active low.
  }
  if (buttonPressed) {
    g_ble.clearBondRecord(true);
    Serial.print("bond cleared (button held at boot)\r\n");
  }

  static const uint8_t kAddress[6] = {0x61, 0x00, 0x15, 0x54, 0xDE, 0xC0};
  bool ok = g_ble.begin(0);
  if (ok) {
    ok = g_ble.setDeviceAddress(kAddress, BleAddressType::kRandomStatic) &&
         g_ble.setAdvertisingPduType(BleAdvPduType::kAdvInd) &&
         g_ble.setAdvertisingName("X54-BOND", true) &&
         g_ble.setScanResponseName("X54-BOND-SCAN") &&
         g_ble.setGattDeviceName("X54 Bond Probe") &&
         g_ble.setGattBatteryLevel(95U);
  }

  Serial.print("BLE init: ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print("\r\n");
  Serial.print("Pair once, disconnect, reconnect, then observe encryption and bond reuse.\r\n");
  Serial.print("Hold user button while rebooting to clear bond.\r\n");
  printBondSummary();
}

void loop() {
  if (!g_ble.isConnected()) {
    if (g_prevConnected) {
      g_prevConnected = false;
      g_prevEncrypted = false;
      g_connectionAnnounced = false;
      Serial.print("disconnected\r\n");
    }

    BleAdvInteraction adv{};
    g_ble.advertiseInteractEvent(&adv, 350U, 350000UL, 700000UL);
    if (adv.receivedConnectInd) {
      g_prevConnected = true;
      g_connectionAnnounced = false;
    } else {
      const uint32_t now = millis();
      if ((now - g_lastAdvLogMs) >= 2000UL) {
        g_lastAdvLogMs = now;
        Serial.print("advertising\r\n");
      }
      if ((now - g_lastBondLogMs) >= 10000UL) {
        g_lastBondLogMs = now;
        printBondSummary();
      }
    }

    Gpio::write(kPinUserLed, true);
    delay(1);
    return;
  }

  BleConnectionEvent evt{};
  const bool ran = g_ble.pollConnectionEvent(&evt, 450000UL);
  if (!g_connectionAnnounced && ran && evt.eventStarted) {
    BleConnectionInfo info{};
    if (g_ble.getConnectionInfo(&info)) {
      Serial.print("connected peer=");
      printAddress(info.peerAddress);
      Serial.print("\r\n");
    } else {
      Serial.print("connected\r\n");
    }
    g_connectionAnnounced = true;
  }
  const bool encrypted = g_ble.isConnectionEncrypted();
  if (encrypted != g_prevEncrypted) {
    g_prevEncrypted = encrypted;
    Serial.print("encryption=");
    Serial.print(encrypted ? "ON" : "OFF");
    if (ran && evt.eventStarted) {
      Serial.print(" ce=");
      Serial.print(evt.eventCounter);
    }
    Serial.print("\r\n");
    if (encrypted) {
      printBondSummary();
    }
  }

  if (ran && evt.terminateInd) {
    Serial.print("link terminated\r\n");
    g_prevConnected = false;
    g_prevEncrypted = false;
    g_connectionAnnounced = false;
  }

  Gpio::write(kPinUserLed, encrypted ? false : true);
  if (!ran) {
    delay(1);
  }
}
