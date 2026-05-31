# BLE Compliance Resume Notes

This file is the handoff point for pausing the full BLE-compliance work. Do not
treat BLE as certification-complete yet; this records what is working, what must
not regress, and what to test first when resuming.

## Current Stable Scope

- [x] Legacy advertising: connectable, scannable, non-connectable, directed
- [x] Active/passive scanning and filtered scan callbacks
- [x] Peripheral and central connections on the clean BLE path
- [x] ATT/GATT server and client through the Bluefruit-compatible API
- [x] NUS/BLEUart receive and notify/write paths
- [x] Bluefruit default central/peripheral link verified at MTU 23 / Data
      Length 27, including central connect callback, CCCD enable, and notify
      delivery on two nRF54 boards
- [x] PHY selection and update APIs: 1M, 2M, Coded S2/S8
- [x] Data Length Extension up to 251 bytes when requested
- [x] ATT MTU exchange up to 247 bytes when requested
- [x] LE Secure Connections Just Works fresh pair, bond save/load, encrypted
      reconnect, encrypted notifications, and encrypted writes on two boards
- [x] LE Secure Connections fixed-PIN and numeric-comparison validation on the
      raw HAL `BlePairPeripheral` / `BlePairCentral` demos, including fresh
      re-pair, bond save/load, and encrypted post-pair GATT traffic
- [x] LE Secure Connections mutual-OOB plumbing through HAL and Bluefruit APIs,
      with compile-tested `BleOobPairPeripheral` / `BleOobPairCentral`
      two-board examples
- [x] Privacy/RPA primitives: HAL and Bluefruit APIs can generate a resolvable
      private address, set it as the local random address, and resolve RPAs
      with the hardware AAR block
- [x] Opt-in local RPA rotation: HAL and Bluefruit APIs can rotate the local
      RPA on a sketch-selected interval before advertising, active scanning,
      or connection initiation while disconnected
- [x] Low-power BLE advertising current is now close to the Zephyr reference
      for the msfujino AdvCurrent test from discussion #71

## Regression-Sensitive Areas

- [ ] Plain non-secure central/peripheral must remain independent from the LE
      Secure Connections path. Issue #68 showed this can regress when security
      changes leak into the unencrypted link setup.
- [ ] Central setup must stay foreground-pumped until the deferred central
      connect callback has run and any active central sync procedure has
      completed. Letting the background connection service take over too early
      can stall the default 23/27 link before CCCD or service discovery
      completes.
- [ ] Default Bluefruit MTU/Data Length behavior must stay Bluefruit-compatible:
      if a sketch does not request a larger value, the user-visible default
      should remain MTU 23 and Data Length 27. Issue #68 covers this.
- [ ] BLEUart/NUS web-device CLI bridge must keep both RX and TX working.
- [ ] Serial and HardwareSerial are not part of BLE but have repeatedly been
      broken by timing changes; do not touch UART code while resuming BLE unless
      the BLE test proves UART is involved.
- [ ] Low-power advertising must not keep RF_SW, HFXO, VPR, Thread, Matter, or
      Zigbee active while idle unless the selected sketch/profile explicitly
      needs them.

## Remaining BLE Compliance Work

- [ ] SMP authenticated LE Secure Connections: passkey entry against non-core
      peers, one-way OOB/NFC/QR host flows, negative/error cases, and broader
      host interoperability
- [ ] Identity, signing, resolving-list policy, automatic peer identity
      selection for bonded reconnects, and bond database behavior against
      phone/desktop hosts
- [ ] Formal ATT/GATT edge cases: long read/write, prepare/execute write,
      descriptor permissions, CCCD persistence, error-code coverage
- [ ] LL control procedure collision handling and broader disconnect reason
      mapping
- [ ] Multi-link stress: simultaneous central/peripheral with mixed MTU/DLE/PHY
      settings
- [ ] Extended advertising and scan-response interoperability beyond local
      smoke tests
- [ ] Connection power profile work; current low-power focus has been
      advertising, not long connected-idle soaks
- [ ] Bluetooth PTS/BQB-style test matrix. The core is not a qualified
      controller.

## Resume Test Matrix

Run these before changing BLE again:

- [ ] Compile `Bluefruit52Lib` BLEUART peripheral and central examples
- [ ] Compile and run the non-secure central/peripheral MTU/DLE pair at:
      MTU 23/Data Length 27, MTU 128/Data Length 132, MTU 247/Data Length 251
- [ ] Compile and run `BlePairPeripheral` + `BlePairCentral` fresh pair
- [ ] Compile and run `BlePairPeripheral` + `BlePairCentral` with
      `-DBLE_PAIR_USE_STATIC_PIN=1`
- [ ] Compile and run `BlePairPeripheral` + `BlePairCentral` with
      `-DBLE_PAIR_USE_NUMERIC_COMPARISON=1` and confirm both boards log the
      same six-digit value before encrypted traffic resumes
- [ ] Compile and run `BleOobPairPeripheral` + `BleOobPairCentral`; exchange
      both printed `peer <r> <c>` lines over Serial and confirm encrypted UART
      traffic resumes after pairing
- [ ] Compile and run `BleResolvablePrivateAddress`; confirm `result=PASS`,
      the printed RPA resolves to local IRK index `0`, phones still see
      the `X54-RPA` advertiser, and the sketch-selected RPA rotation interval
      does not interrupt an active connection
- [ ] Reboot both boards and confirm bonded encrypted reconnect without clearing
      storage
- [ ] Run BLEUart/NUS against Makerdiary Web Device CLI and verify RX and TX
- [ ] Run AdvCurrent in `PowerProfile: WFI`, VPR off, Thread off, Matter off,
      Zigbee off, and compare against the Zephyr reference
- [ ] Repeat one compile/install test from a freshly installed board package,
      not only from the local source tree

## Current Power Finding From Discussion #71

The latest reporter result on May 17, 2026 says the large current spikes were
caused by a loose PPK2/battery-pad connection. With that fixed, v0.7.11 is at
roughly the same level as Zephyr, but the Arduino path still had avoidable
charge before each foreground connectable/scannable advertising event.

Root cause found in the Arduino path:

- Bluefruit foreground advertising calls `advertiseInteractEvent()`.
- `advertiseInteractEvent()` applies the BLE random advertising delay before
  starting the radio event.
- Bluefruit did not move `next_adv_due_us_` into the future until after the
  radio event returned.
- During the random delay, `delay()` saw advertising as overdue and refused WFI,
  so the CPU could busy-spin for up to the random advertising delay before TX.

Fix applied:

- Bluefruit foreground advertising now applies the BLE random advertising delay
  in the scheduler and calls the radio without the extra internal random-delay
  sleep. This preserves random spacing between advertising events but removes
  the visible pre-TX CPU plateau.

Do not remove the random advertising delay to chase current. It is part of BLE
advertising timing behavior. The correct fix is to sleep through it.
