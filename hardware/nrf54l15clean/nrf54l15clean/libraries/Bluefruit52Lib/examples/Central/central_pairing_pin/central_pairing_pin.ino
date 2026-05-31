/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <bluefruit.h>

/* This sketch demonstrates central-side pairing with a fixed six-digit PIN.
 * Use it with the Security > pairing_pin peripheral example.
 *
 * The local side is configured as Keyboard Only plus a fixed passkey.
 * That lets the compatibility layer feed the static PIN into the SMP
 * passkey-entry path without any runtime user input on the board.
 */

#define PAIRING_PIN "123456"

BLEClientUart clientUart;

static bool g_uartReady = false;

void scan_callback(ble_gap_evt_adv_report_t* report);
void connect_callback(uint16_t conn_handle);
void disconnect_callback(uint16_t conn_handle, uint8_t reason);
void bleuart_rx_callback(BLEClientUart& uart_svc);
void pairing_complete_callback(uint16_t conn_handle, uint8_t auth_status);
void connection_secured_callback(uint16_t conn_handle);

void setup() {
  Serial.begin(115200);
  Serial.println("Bluefruit52 Central Fixed PIN Pairing Example");
  Serial.println("---------------------------------------------");

  Bluefruit.begin(0, 1);

  // Keyboard-only + fixed PIN forces passkey entry instead of Just Works.
  Bluefruit.Security.setIOCaps(false, false, true);
  Bluefruit.Security.setPIN(PAIRING_PIN);
  Bluefruit.Security.setPairCompleteCallback(pairing_complete_callback);
  Bluefruit.Security.setSecuredCallback(connection_secured_callback);

  clientUart.begin();
  clientUart.setRxCallback(bleuart_rx_callback);

  Bluefruit.setConnLedInterval(250);
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);

  Serial.print("Scanning for pairing_pin peripheral, fixed PIN = ");
  Serial.println(PAIRING_PIN);
}

void scan_callback(ble_gap_evt_adv_report_t* report) {
  if (Bluefruit.Scanner.checkReportForService(report, clientUart)) {
    Serial.println("BLE UART service detected. Connecting ...");
    Bluefruit.Central.connect(report);
    return;
  }

  Bluefruit.Scanner.resume();
}

void connect_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  g_uartReady = false;

  Serial.println("Connected");
  if (conn != nullptr && !conn->bonded()) {
    Serial.println("Requesting pairing");
    conn->requestPairing();
  }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  g_uartReady = false;
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
}

void bleuart_rx_callback(BLEClientUart& uart_svc) {
  while (uart_svc.available()) {
    Serial.write(static_cast<uint8_t>(uart_svc.read()));
  }
}

void pairing_complete_callback(uint16_t conn_handle, uint8_t auth_status) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  if (auth_status == BLE_GAP_SEC_STATUS_SUCCESS) {
    Serial.println("Pairing succeeded");
  } else {
    Serial.print("Pairing failed, status = 0x");
    Serial.println(auth_status, HEX);
    if (conn != nullptr) {
      conn->disconnect();
    }
  }
}

void connection_secured_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  if (conn == nullptr) {
    return;
  }

  if (!conn->secured()) {
    Serial.println("Bond restore failed, requesting fresh pairing");
    conn->requestPairing();
    return;
  }

  Serial.println("Link secured");
  Serial.print("Encrypted: ");
  Serial.println(Bluefruit.Security.isEncrypted(conn_handle) ? "yes" : "no");
  Serial.print("Authenticated MITM: ");
  Serial.println(conn->authenticated() ? "yes" : "no");
  if (!conn->authenticated()) {
    Serial.println("Link is encrypted but not authenticated; requesting fresh pairing");
    conn->requestPairing();
    return;
  }

  Serial.print("Discovering BLE UART service ... ");
  if (clientUart.discover(conn_handle)) {
    Serial.println("Found it");
    clientUart.enableTXD();
    g_uartReady = true;
    Serial.println("Secure BLE UART ready");
  } else {
    Serial.println("Found NONE");
    conn->disconnect();
  }
}

void loop() {
  BLEConnection* conn = Bluefruit.Connection(0);
  if (conn == nullptr || !conn->connected() || !conn->secured() || !g_uartReady) {
    return;
  }

  if (Serial.available()) {
    delay(2);
    char str[21] = {0};
    Serial.readBytes(str, 20);
    clientUart.print(str);
  }
}
