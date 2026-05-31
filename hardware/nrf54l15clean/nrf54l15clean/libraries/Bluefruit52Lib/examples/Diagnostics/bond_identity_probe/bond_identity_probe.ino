/*
  Bond identity probe

  Pairs as a BLE UART peripheral with a fixed six-digit PIN, then prints the
  stored bond address, peer identity address, peer IRK presence, and
  authenticated-link state. This is useful when testing phone/desktop hosts
  that rotate resolvable private addresses between connections.
*/

#include <bluefruit.h>

#define PAIRING_PIN "123456"

BLEUart bleuart;

void startAdv();
void connectCallback(uint16_t conn_handle);
void disconnectCallback(uint16_t conn_handle, uint8_t reason);
void securedCallback(uint16_t conn_handle);
void pairingCompleteCallback(uint16_t conn_handle, uint8_t auth_status);
void printBondState();
void printAddress(const char* label, const ble_gap_addr_t& addr);
void printHex16(const char* label, const uint8_t value[16]);

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 1500UL) {
    delay(10);
  }

  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setName("X54-BOND");
  Bluefruit.setTxPower(0);

  Bluefruit.Security.setPIN(PAIRING_PIN);
  Bluefruit.Security.setPairCompleteCallback(pairingCompleteCallback);
  Bluefruit.Security.setSecuredCallback(securedCallback);

  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  bleuart.setPermission(SECMODE_ENC_WITH_MITM, SECMODE_ENC_WITH_MITM);
  bleuart.begin();

  Serial.println("Bond identity probe");
  Serial.println("Pairing PIN: " PAIRING_PIN);
  printBondState();
  startAdv();
}

void startAdv() {
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.Advertising.addName();
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  while (Serial.available() > 0) {
    delay(2);
    uint8_t buffer[32] = {0};
    const int count = Serial.readBytes(buffer, sizeof(buffer));
    if (count > 0) {
      bleuart.write(buffer, static_cast<size_t>(count));
    }
  }

  while (bleuart.available() > 0) {
    const int ch = bleuart.read();
    if (ch >= 0) {
      Serial.write(static_cast<uint8_t>(ch));
    }
  }
}

void connectCallback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char peer[32] = {0};
  if (connection != nullptr && connection->getPeerName(peer, sizeof(peer))) {
    Serial.print("Connected to ");
    Serial.println(peer);
  } else {
    Serial.println("Connected");
  }
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
}

void securedCallback(uint16_t conn_handle) {
  Serial.println("Secured");
  Serial.print("encrypted=");
  Serial.println(Bluefruit.Security.isEncrypted(conn_handle) ? "yes" : "no");
  Serial.print("authenticated=");
  Serial.println(Bluefruit.Security.isAuthenticated(conn_handle) ? "yes" : "no");

  if (Bluefruit.Security.addBondedPeerIrkToResolvingList()) {
    Serial.print("bonded_peer_irk_added=yes count=");
    Serial.println(Bluefruit.Security.resolvingListCount());
  } else {
    Serial.println("bonded_peer_irk_added=no");
  }

  printBondState();
}

void pairingCompleteCallback(uint16_t conn_handle, uint8_t auth_status) {
  (void)conn_handle;
  if (auth_status == BLE_GAP_SEC_STATUS_SUCCESS) {
    Serial.println("Pairing succeeded");
  } else {
    Serial.print("Pairing failed, status = 0x");
    Serial.println(auth_status, HEX);
  }
}

void printBondState() {
  Serial.print("has_bond=");
  Serial.println(Bluefruit.Security.hasBond() ? "yes" : "no");
  if (!Bluefruit.Security.hasBond()) {
    return;
  }

  Serial.print("bond_authenticated=");
  Serial.println(Bluefruit.Security.bondAuthenticated() ? "yes" : "no");

  ble_gap_addr_t addr{};
  if (Bluefruit.Security.getBondPeerAddress(&addr)) {
    printAddress("bond_peer_addr", addr);
  }
  if (Bluefruit.Security.getBondPeerIdentityAddress(&addr)) {
    printAddress("bond_peer_identity", addr);
  } else {
    Serial.println("bond_peer_identity=none");
  }

  uint8_t peerIrk[16] = {0};
  if (Bluefruit.Security.getBondPeerIrk(peerIrk)) {
    printHex16("bond_peer_irk", peerIrk);
  } else {
    Serial.println("bond_peer_irk=none");
  }
}

void printAddress(const char* label, const ble_gap_addr_t& addr) {
  Serial.print(label);
  Serial.print(" type=");
  Serial.print(addr.addr_type);
  Serial.print(" value=");
  for (int i = 5; i >= 0; --i) {
    if (addr.addr[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(addr.addr[i], HEX);
    if (i > 0) {
      Serial.print(':');
    }
  }
  Serial.println();
}

void printHex16(const char* label, const uint8_t value[16]) {
  Serial.print(label);
  Serial.print('=');
  for (uint8_t i = 0U; i < 16U; ++i) {
    if (value[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(value[i], HEX);
  }
  Serial.println();
}
