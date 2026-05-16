# BLE Secure Connections Path Analysis

This note records the LE Secure Connections path that is now implemented and
tested in the `nrf54l15clean` core. It is not a Bluetooth qualification claim;
it is a practical controller-path verification against two XIAO nRF54L15 boards.

## Tested Scope

- Peripheral: `BlePairPeripheral`
- Central: `BlePairCentral`
- Build options: BLE trace on, VPR off, Thread off, Matter off, Zigbee off
- Security mode: LE Secure Connections Just Works
- Persistence: Arduino `Preferences` callback storage plus core bond record
- Traffic after encryption: notification stream plus repeated ON/OFF writes

## Expected Protocol Sequence

The working sequence is:

1. Peripheral sends `SMP Security Request`.
2. Central sends `SMP Pairing Request`.
3. Peripheral sends `SMP Pairing Response`.
4. Public keys, confirm values, random values, and DHKey checks are exchanged.
5. Both sides persist the SC-derived LTK as the bond key.
6. Central starts link-layer encryption with `LL_ENC_REQ`.
7. Peripheral replies with `LL_ENC_RSP`.
8. Central sends `LL_START_ENC_REQ`.
9. Peripheral replies with `LL_START_ENC_RSP`.
10. Encrypted ATT writes and notifications run over the encrypted link.
11. After reboot, both sides load the saved bond and reconnect encrypted without
    doing a new SMP pairing exchange.

## Important Implementation Details

- LE Secure Connections now negotiates zero distributed keys for the SC path.
  SC derives the LTK from DHKey locally; requesting optional ID/signing key
  packets before the core transmits them could leave bond persistence waiting on
  key material that never arrives.
- Both central connection setup paths now prime the loaded bond before the first
  connection events, so bonded reconnects can queue `LL_ENC_REQ`.
- `sendSmpSecurityRequest()` is valid for a bonded but unencrypted peripheral
  connection. That lets a peripheral ask a bonded central to re-enable
  encryption instead of starting a new pairing procedure.
- A bonded central that receives `SMP Security Request` now starts encryption if
  the link is idle, and ignores the request if encryption is already queued or
  active. That avoids duplicate encryption-start procedures.

## Verified Result

Two-board hardware testing confirmed:

- fresh SC pairing saves bonds on both sides
- encryption reaches `ON` on both sides
- encrypted notifications are received by the central
- encrypted ON/OFF writes are received by the peripheral
- `mic=0` during the tested runs
- `disc_dbg none` at the end of the fresh-pair run
- after reboot without clearing storage, both boards load the saved bond
- bonded reconnect comes back encrypted without a new pairing exchange
- encrypted notify/write traffic continues after bonded reconnect

## Remaining Caveats

- This is still a clean register-level BLE implementation, not Nordic SoftDevice,
  Zephyr, or a Bluetooth-qualified controller.
- The tested SC mode is Just Works. Numeric comparison, passkey entry, OOB, key
  distribution for identity/signing keys, privacy address resolution at full host
  stack level, and formal qualification remain future work.
- More phone, Windows, Linux, and macOS host interoperability testing is still
  needed before calling the BLE controller broadly conformant.
