## Bluefruit52Lib Compatibility Layer

`Bluefruit52Lib` is the bundled nRF52/nRF52840 compatibility library for the
XIAO nRF54L15 clean core.

It keeps the familiar Bluefruit-style API available on top of the clean nRF54
HAL, including `Bluefruit`, `BLEUart`, `BLEClientUart`, scanner/client
helpers, Device Information, Battery Service, and common advertising helpers.

Examples in Arduino IDE:

- `File -> Examples -> Bluefruit52Lib -> Advertising`
- `File -> Examples -> Bluefruit52Lib -> Central`
- `File -> Examples -> Bluefruit52Lib -> Diagnostics`
- `File -> Examples -> Bluefruit52Lib -> DualRoles`
- `File -> Examples -> Bluefruit52Lib -> HID`
- `File -> Examples -> Bluefruit52Lib -> nRF52Compat`
- `File -> Examples -> Bluefruit52Lib -> Peripheral`
- `File -> Examples -> Bluefruit52Lib -> Projects`
- `File -> Examples -> Bluefruit52Lib -> Security`
- `File -> Examples -> Bluefruit52Lib -> Services`

`nRF52Compat` is the starter pack for direct sketch-porting examples:

- `central_bleuart`
- `central_scan`
- `central_notify`
- `dual_bleuart`
- `beacon`
- `custom_hrm`
- `notify_peripheral`
- `pairing_pin`

These are unchanged upstream-style sketches included locally because they are
known to compile on the nRF54 wrapper and give users concrete migration
starting points.

For the simplest custom notification flow, use `notify_peripheral` together
with `central_notify`.

BLE PHY requests are available through the nRF52840-style connection object:

```cpp
void connect_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  conn->requestPHY(BLE_GAP_PHY_2MBPS);  // 1: 1M, 2: 2M, 4: coded
  uint8_t phy = conn->getPHY();
}
```

`requestPHY()` can be called directly from the connect callback. The
compatibility layer queues it safely after the callback returns.

Authenticated pairing state is also exposed for security tests:

```cpp
void connection_secured_callback(uint16_t conn_handle) {
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  bool encrypted = Bluefruit.Security.isEncrypted(conn_handle);
  bool authenticated = (conn != nullptr) && conn->authenticated();
}
```

`authenticated()` is only true after an encrypted MITM-capable link, such as
fixed-PIN, numeric-comparison, or OOB pairing. Plain encrypted Just Works links
remain `secured()` but not authenticated.

For bonded peers, CCCD subscriptions are restored automatically when the saved
bond identity matches the reconnecting peer. This lets notify/indicate
characteristics keep the same Bluefruit callback behavior after a bonded
reconnect without requiring the central to rewrite every CCCD immediately.

Bond identity helpers are available for host privacy debugging:

```cpp
ble_gap_addr_t identity = {};
uint8_t peerIrk[16] = {};
bool hasIdentity = Bluefruit.Security.getBondPeerIdentityAddress(&identity);
bool hasIrk = Bluefruit.Security.getBondPeerIrk(peerIrk);
bool inResolver = Bluefruit.Security.addBondedPeerIrkToResolvingList();
```

Use `Diagnostics > bond_identity_probe` to pair with a phone or desktop host
and print the saved peer address, identity address, IRK presence, and
authenticated bond flag.

Use `Diagnostics > gatt_descriptor_helpers` to inspect custom GATT descriptor
helpers. It exposes readable `0x2901`, `0x2904`, and `0x2908` descriptors plus a
writable `0x2901` User Description descriptor that can be written and read back
from a BLE scanner.

HID Protocol Mode changes are visible to sketches. `BLEHidAdafruit` switches
keyboard/mouse notifications between Report and Boot characteristics when a host
writes Protocol Mode:

```cpp
blehid.setProtocolModeCallback([](uint16_t conn, uint8_t mode) {
  (void) conn;
  Serial.println(mode == BLE_HID_PROTOCOL_MODE_BOOT ? "Boot" : "Report");
});
uint8_t mode = blehid.protocolMode();
```

Keyboard LED output reports are also visible. Hosts write NumLock, CapsLock,
ScrollLock, Compose, and Kana bits to the HID output report or boot keyboard
output report. Sketches can use a callback or poll the last received state:

```cpp
blehid.setKeyboardLedCallback([](uint16_t conn, uint8_t leds) {
  (void) conn;
  Serial.print("Keyboard LEDs: 0x");
  Serial.println(leds, HEX);
});
uint8_t leds = blehid.keyboardLedState();
```

The broader Bluefruit menus now ship the practical wrapper examples by role:

- `Advertising`: `adv_advanced`, `beacon`, `eddystone_url`
- `Central`: `central_bleuart_multi`, `central_custom_hrm`, `central_hid`, `central_pairing`, `central_scan_advanced`, `central_throughput`
- `Diagnostics`: `bond_identity_probe`, `gatt_descriptor_helpers`, `throughput`, `rssi_callback`, `rssi_poll`
- `DualRoles`: `dual_bleuart`
- `HID`: `blehid_keyboard`, `blehid_mouse`, `blehid_gamepad`, `blehid_camerashutter`
- `Projects`: `rssi_proximity_central`, `rssi_proximity_peripheral`
- `Security`: `pairing_passkey`, `pairing_pin`, `clearbonds`
- `Services`: `bleuart`, `bleuart_multi`, `custom_hrm`, `custom_htm`, `client_cts`, `ancs`

The supported surface is the shipped example set above. Common BLE UART,
scanner, custom notify, and central discovery flows are validated on the nRF54
wrapper and are the recommended starting point for nRF52 sketch ports.
