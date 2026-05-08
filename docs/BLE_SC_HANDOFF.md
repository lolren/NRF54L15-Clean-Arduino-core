# BLE Secure Connections Handoff

This document is the current handoff for BLE LE Secure Connections (LESC)
work in the custom nRF54L15 Arduino core.

It is written for another engineer or another AI that needs to continue the
work without re-discovering the state from scratch.

It covers:

- what exists today
- what is known to work
- what still fails
- how the current controller is structured
- which files matter most
- how to compile and run the current test harnesses
- how to interpret the logs
- what hypotheses are still worth pursuing
- what limitations are real and what limitations are just current
  implementation gaps

This is intentionally long. The cost of reading this is lower than the cost of
another blind BLE SC debugging loop.

## Scope

This document is about the custom BLE controller and BLE security path in:

- [nrf54l15_hal.h](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal.h)
- [nrf54l15_hal.cpp](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal.cpp)
- [nrf54l15_hal_parts](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts)

This is not about:

- Matter or Thread, except where software P-256 timing affects BLE SC
- BlueZ host bugs, except where host instability contaminates evidence
- smartphone UX or application-layer pairing policies

## High-Level Situation

The core already supports:

- advertising
- scanning
- connections
- basic ATT/GATT
- legacy pairing / bonding flows to some degree
- bond persistence probes
- central and peripheral roles

LE Secure Connections is partially implemented and partially validated.

The custom controller now gets substantially further than the old legacy-only
fallback:

- it can exchange SC pairing messages
- it can do software P-256 key generation and ECDH fast enough to participate
- it can reach encrypted link state
- it can perform encrypted ATT subscribe / notify / write traffic on two boards
- it can do several successful duplex application cycles after encryption

The unresolved problem is not "SC cannot start at all" anymore.

The unresolved problem is:

- long-run reliability after successful SC start, especially during sustained
  encrypted duplex traffic with queued ATT traffic on both sides
- intermittent reconnect stability across repeated SC sessions

That distinction matters. The work is no longer at the "SMP math missing"
phase. The work is at the "controller event progression / encrypted steady
state / queued traffic carry-over" phase.

## Current Practical Status

### What is confirmed to work

The code on `main` has already moved beyond a fake or legacy-only response.

The following are confirmed from prior hardware work on two attached XIAO
nRF54L15 boards:

- SC pairing handshake reaches encrypted state
- central reports `encryption=ON`
- peripheral reports `encryption=ON`
- central can subscribe to the peripheral's notify characteristic
- central can send encrypted writes to the peripheral
- peripheral can send encrypted notifications to the central
- both directions can work in the same session
- multiple notify/write cycles can succeed before failure

Observed successful application-level markers included:

- central logs such as `ble_pair central: subscribed`
- central logs such as `ble_pair central: notify=ping-1`
- later successful runs reaching roughly `notify=ping-40`
- peripheral logs such as `ble_pair gatt-write val=ON`
- peripheral logs such as `ble_pair gatt-write val=OFF`

So the question is not whether secure traffic can ever flow. It can.

### What is still broken

The two-board secure-duplex soak is still not stable enough to call finished.

The remaining failure is typically a supervision timeout after a later
steady-state encrypted exchange, not an immediate SMP setup failure.

Historically observed failure signatures:

1. Handshake-era failure:
   - intermittent SC start-encryption timing failures around
     `lasttx_op=0x05` / `lastrx_op=0x04`
   - these correspond to `LL_START_ENC_REQ` / `LL_START_ENC_RSP` territory

2. Post-subscribe / post-write-response failure:
   - queueing worked, first encrypted app traffic worked, then the link stalled

3. Later duplex steady-state failure:
   - both sides still had pending encrypted application data
   - central typically ended with:
     - pending TX valid
     - pending LLID `2`
     - pending payload length `8`
     - `lasttx_op=0x52`
     - `lastrx_op=0x1B`
     - `ack=1`
   - peripheral typically ended with:
     - pending TX valid
     - pending LLID `2`
     - pending payload length `14`
     - `lasttx_op=0x1B`
     - `lastrx_op=0x52`
     - `ack=1`

That signature strongly suggests the unresolved seam is not "bad keys" or
"cannot decrypt", but "event-to-event progression when both sides have queued
encrypted ATT traffic".

## Most Important Conclusion

The current BLE SC problem is best described as:

- handshake mostly works
- first encrypted ATT operations work
- several later encrypted ATT operations work
- long-run controller scheduling / ACK / carry-over behavior is still wrong in
  some sequences

That should drive the next debugging strategy. Do not restart from the
assumption that the controller lacks SC entirely.

## Relevant Files

### Security core

- [nrf54l15_hal_ble_ll_security.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_ll_security.inc)
- [nrf54l15_hal_ble_att_l2cap.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_att_l2cap.inc)
- [nrf54l15_hal_ble_custom_gatt.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_custom_gatt.inc)

### Central event path

- [nrf54l15_hal_ble_central_event.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_central_event.inc)

### Peripheral event path

- [nrf54l15_hal_ble_peripheral_event_rx.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_rx.inc)
- [nrf54l15_hal_ble_peripheral_event_tx.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tx.inc)
- [nrf54l15_hal_ble_peripheral_event_tail.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tail.inc)

### Crypto / ECC

- [matter_secp256r1.cpp](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/matter_secp256r1.cpp)

This file is used by Matter, but the same software P-256 implementation also
matters for BLE Secure Connections timing because there is no fast hardware ECC
path available in the public core.

### Debug structs and public debug state

- [nrf54l15_hal.h](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal.h)

Important exported debug structures there:

- `BleEncryptionDebugCounters`
- `BleSecureConnectionsDebugState`
- `BleDisconnectDebug`

### Test examples

- [BlePairCentral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral/BlePairCentral.ino)
- [BlePairPeripheral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral/BlePairPeripheral.ino)
- [BlePairingEncryptionStatus.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairingEncryptionStatus/BlePairingEncryptionStatus.ino)
- [BleBondPersistenceProbe.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BleBondPersistenceProbe/BleBondPersistenceProbe.ino)
- [BleTwoBoardPairingBondingDemo.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BleTwoBoardPairingBondingDemo/BleTwoBoardPairingBondingDemo.ino)

### Existing runbook/script

- [BLE_REGRESSION_RUNBOOK.md](./BLE_REGRESSION_RUNBOOK.md)
- [ble_pair_bond_regression.sh](../scripts/ble_pair_bond_regression.sh)

## Test Assets and Roles

### Primary two-board harness

Use:

- [BlePairPeripheral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral/BlePairPeripheral.ino)
  on the peripheral board
- [BlePairCentral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral/BlePairCentral.ino)
  on the central board

Those files already have hardcoded roles:

- `BlePairPeripheral.ino`: `ROLE = PERIPHERAL`
- `BlePairCentral.ino`: `ROLE = CENTRAL`

Do not waste time editing the `ROLE` constant unless you have a specific reason
to collapse both roles into one file during a new experiment.

### Why this harness is the primary one

These examples are the most useful because they combine:

- bond persistence
- SC debug
- disconnect debug
- application duplex traffic
- custom notify and write characteristics
- queue interaction on both sides

This is the shortest path to reproducing the remaining bug.

### Secondary host-based harness

Use:

- [BlePairingEncryptionStatus.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairingEncryptionStatus/BlePairingEncryptionStatus.ino)
- [BleBondPersistenceProbe.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BleBondPersistenceProbe/BleBondPersistenceProbe.ino)

These are useful for:

- host adapter pairing/bonding regressions
- BlueZ / btmon capture
- bond restore behavior
- encryption change event classification

These are less useful than the two-board harness for the current SC steady-state
bug because the remaining bug is now deep in duplex controller traffic, not only
in the initial host-driven pairing path.

## Build and Upload Commands

### Recommended local override flow

The cleanest way to test the repo directly without waiting for Boards Manager
packaging is a sketchbook override in a temporary user directory.

From the repo root:

```bash
mkdir -p /tmp/arduino-user/hardware
rm -rf /tmp/arduino-user/hardware/nrf54l15clean
ln -s "$PWD/hardware/nrf54l15clean" /tmp/arduino-user/hardware/nrf54l15clean
```

Then use a temporary config:

```bash
tmpcfg=$(mktemp)
cat > "$tmpcfg" <<'YAML'
directories:
  user: /tmp/arduino-user
  data: /home/lolren/.arduino15
YAML
```

### Compile commands

Peripheral:

```bash
arduino-cli compile \
  --config-file "$tmpcfg" \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral
```

Central:

```bash
arduino-cli compile \
  --config-file "$tmpcfg" \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral
```

### Useful FQBN menu options

Trace on:

```text
nrf54l15clean:nrf54l15clean:xiao_nrf54l15:clean_ble_trace=on
```

Default BLE timing profile:

- `clean_ble_timing=interop`

Do not start by changing the BLE timing menu unless you are testing that
specifically. The interop default is the least misleading baseline.

### Upload commands

First identify boards:

```bash
arduino-cli board list
python3 - <<'PY'
from serial.tools import list_ports
for p in list_ports.comports():
    print(p.device, p.serial_number, p.description)
PY
```

Upload peripheral:

```bash
arduino-cli upload \
  --config-file "$tmpcfg" \
  -p /dev/ttyACM0 \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral
```

Upload central:

```bash
arduino-cli upload \
  --config-file "$tmpcfg" \
  -p /dev/ttyACM1 \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral
```

Observed mapping in one recent session was:

- `/dev/ttyACM0` -> probe UID `761FDE87`
- `/dev/ttyACM1` -> probe UID `E91217E8`

Do not assume that remains true forever. Detect first.

## How to Drive the Two-Board Harness

Open two serial monitors at `115200`.

At boot you should see:

- peripheral:
  - `ble_pair === BLE Pairing Demo (PERIPHERAL) ===`
  - `ble_pair gatt+adv: ready`
- central:
  - `ble_pair === BLE Pairing Demo (CENTRAL) ===`

The central scans and connects automatically.

The peripheral advertises automatically.

After connect:

- peripheral should send an SMP Security Request if no bond exists
- central should progress into pairing
- both sides should eventually print `encryption=ON`

After encryption:

- central subscribes to the peripheral notify characteristic
- central writes alternating `ON` / `OFF`
- peripheral periodically emits `ping-N` notifications

Expected happy-path markers:

- `ble_pair connected`
- `ble_pair sent-smp-security-request=1`
- `ble_pair central: connected`
- `ble_pair encryption=ON`
- `ble_pair central: encryption=ON`
- `ble_pair central: subscribed`
- `ble_pair gatt-write val=ON`
- `ble_pair gatt-write val=OFF`
- `ble_pair central: notify=ping-1`

## Serial Commands Inside the Harness

Both primary examples expose serial commands.

Command list:

- `status`
- `clear`
- `sc`
- `enc`
- `disc`
- `trace`
- `keyprobe`

Meanings:

- `status`
  - prints high-level connection / encryption / bond / RSSI status
- `clear`
  - clears bond state
  - this is important: it does not merely clear logs
- `sc`
  - prints `BleSecureConnectionsDebugState`
- `enc`
  - prints `BleEncryptionDebugCounters`
- `disc`
  - prints `BleDisconnectDebug`
- `trace`
  - dumps the in-memory BLE trace ring
- `keyprobe`
  - forces a software secp256r1 keypair generation and prints the public key
  - useful as a quick sanity check that ECC is alive, but not enough to prove
    the controller path is correct

## Important Gotchas

### 1. `clear` clears bond state

This matters a lot. During previous debugging it was easy to use `clear` as if
it only reset traces. It does not. It changes the test path by deleting the
bond.

If you are doing reconnect testing, do not casually issue `clear`.

### 2. The central example uses fixed handles

The central-side test harness uses fixed peer handles in
[BlePairCentral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral/BlePairCentral.ino):

- notify value handle `0x0022`
- notify CCCD handle `0x0023`
- write handle `0x0025`

Those are valid only because the peripheral example builds the matching custom
GATT table.

Do not use this example as if it were a generic GATT client.

### 3. `BLE_PAIR_CENTRAL_WRITE_WITH_RESPONSE` is not the default path

The characteristic properties are currently:

- notify characteristic: read + notify
- write characteristic: write without response

If you flip `BLE_PAIR_CENTRAL_WRITE_WITH_RESPONSE=1` without adjusting the
characteristic design, you can generate confusing ATT errors that are
self-inflicted.

### 4. Host-driven regressions can be contaminated

The host-side regression script classifies:

- host instability
- MIC failure
- target-side failure

Do not overfit to a single BlueZ/adapter run when the two-board harness already
shows the controller can do more than the host trace implies.

### 5. The controller is custom, not Zephyr/NimBLE/SoftDevice

Do not assume there is a hidden full stack under the hood.

The behavior is whatever this controller code does.

That is both a weakness and an advantage:

- weakness: more manual LL/ATT/SMP work
- advantage: every failure is visible in the code and can be instrumented

## Current Debug Surfaces

### `BleEncryptionDebugCounters`

Use this when you need:

- TX/RX encryption state breadcrumbs
- MIC failure data
- follow-up RX timing behavior
- LL start-encryption counters
- disconnect/missed-event timing crumbs

Important fields:

- `mainEncReqSeen`
- `mainEncRspTxOk`
- `mainStartEncReqSeen`
- `mainStartEncReqSeenDecrypted`
- `mainStartEncRspTxOk`
- `encRxMicFailCount`
- `encLastTxWasEncrypted`
- `encLastRxWasDecrypted`
- `connLatePollCount`
- `connRxTimeoutCount`
- `connFollowupRxTimeoutCount`
- `connMissedEventCountLast`
- `connMissedEventCountMax`
- `connLastDisconnectReason`

### `BleSecureConnectionsDebugState`

Use this to inspect logical SC state:

- whether peer public key was received
- whether local public key was sent
- whether confirm/random/DHKey Check are sent/received
- keypair time
- DH time
- check-values time
- cooperate hook count

Important fields:

- `active`
- `localInitiator`
- `peerPublicKeyValid`
- `publicKeySent`
- `confirmSent`
- `randomSent`
- `dhKeyReady`
- `checkValuesReady`
- `dhKeyCheckSent`
- `receivedDhKeyCheckValid`
- `pendingTxValid`
- `pairingState`
- `localKeypairTimeUs`
- `dhKeyTimeUs`
- `checkValuesTimeUs`

### `BleDisconnectDebug`

Use this to characterize steady-state failures.

Especially useful:

- `reason`
- `eventCounter`
- `missedEventCount`
- `pendingTxValid`
- `pendingTxLlid`
- `pendingTxLength`
- `lastTxOpcode`
- `lastRxOpcode`
- `lastPeerAckedLastTx`

## Where the Remaining Bug Probably Lives

The strongest current suspicion is not the ECC math and not the early SMP
packets.

The strongest current suspicion is the interaction between:

- central event progression
- `connectionFreshTxAllowed_`
- queued ATT traffic promotion
- same-event ACK / turnaround handling
- encrypted retransmission reuse
- carry-over to the next event when both sides still have pending data

The highest-value file for this is:

- [nrf54l15_hal_ble_central_event.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_central_event.inc)

Secondary high-value files:

- [nrf54l15_hal_ble_peripheral_event_tx.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tx.inc)
- [nrf54l15_hal_ble_peripheral_event_rx.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_rx.inc)

### Why central event logic is still suspect

The central code explicitly has a comment that it does not promote a queued
central request after an earlier empty ACK:

```cpp
// Do not replace an earlier empty ACK with a fresh queued central request.
// Reusing the same SN with a different ATT/L2CAP payload is fragile...
const bool promotePendingTxAfterEmptyAck = false;
```

That conservative choice may avoid one class of bug while leaving another:

- central may not progress quickly enough when both sides have queued traffic
- same-event and next-event request promotion might still be too rigid

This does not prove that line should be changed blindly. It does show that the
remaining problem is very likely in this area of the design.

## Why ECC Is Not the Main Blocker Anymore

Software P-256 used to be a massive blocker. It was sped up significantly.

The main improvements already made were in:

- [matter_secp256r1.cpp](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/matter_secp256r1.cpp)

Past measured numbers from hardware work were roughly:

- keygen: about `20.9 s` -> about `0.8 s`
- sign: about `22.5 s` -> about `0.8 s`
- verify: about `47.3 s` -> about `1.7 s`
- ECDH: about `24.1 s` -> about `0.9 s`

For BLE SC that means:

- software ECC is still expensive
- but it is no longer so expensive that every failure should be blamed on math
  time alone

There is still headroom for more ECC work, but the remaining SC bug is already
past the point where "no usable ECDH" explains everything.

## What Is Already Fixed and Should Not Be Re-litigated First

The next engineer should not start by redoing these blindly:

- public-key wire-format normalization
- DHKey byte-order fix for `f5` / `f6`
- basic responder `LL_START_ENC_RSP` encryption enable timing
- duplicate pairing request / response tolerance
- central post-notification same-event ACK work
- hardware CCM header masking bug for encrypted BLE header authentication

These areas already had meaningful work. They may still contain edge bugs, but
they are not "untouched territory".

## Recommended Next Investigation Order

1. Reproduce on the two-board harness, not on a phone first.
2. Keep the peripheral example on [BlePairPeripheral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral/BlePairPeripheral.ino).
3. Keep the central example on [BlePairCentral.ino](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral/BlePairCentral.ino).
4. Use `BLE_PAIR_QUIET_TEST=1` first to reduce log noise.
5. Confirm:
   - connect
   - `encryption=ON`
   - `subscribed`
   - at least one `notify=ping-N`
   - at least one `gatt-write val=ON/OFF`
6. Let it soak until failure.
7. Capture `enc`, `disc`, and `trace` on both sides immediately after failure.
8. Compare whether both sides still had pending encrypted ATT traffic.
9. Focus on:
   - queued TX promotion
   - ACK interpretation
   - event-start state after a previous notification/write exchange
   - whether both sides are waiting for the other to spend the next event

## Concrete Experiments Worth Doing

### Experiment 1: Force single-direction encrypted traffic

Run each mode separately:

- notify only
- write only
- notify + write

The bug historically became sharpest in duplex mode. Verify whether the current
build still separates cleanly that way.

### Experiment 2: Change queue pressure

Vary:

- `kNotifyMs`
- central write interval
- background connection service enable/disable

Goal:

- determine whether the bug is caused by queue density or by a specific packet
  order

### Experiment 3: Instrument promotion decisions

Add one-line traces around:

- when a queued ATT request is promoted
- when an empty ACK is sent instead
- when `connectionFreshTxAllowed_` flips
- when `peerAckedLastTx` is inferred rather than explicit
- when pending TX remains armed into the next event

The goal is not more generic logs. The goal is to prove why one side still
thinks it cannot send the queued payload.

### Experiment 4: Temporary simplification

Temporarily disable one of these at a time:

- custom notify queueing
- implicit ACK heuristics
- same-event follow-up send path
- background connection servicing

Do not ship these simplifications. Use them to isolate the smallest subsystem
that changes the failure mode.

### Experiment 5: Verify reconnect stability separately

The reconnect path and the long-run duplex path are related but not identical.

A good final fix should improve both, but during debugging they should be
measured separately.

## Known Limitations

These are real limitations today:

- no public hardware ECC offload path
- custom BLE controller instead of a mature upstream controller stack
- minimal central model, not a fully generic BLE host stack
- examples are highly targeted, especially fixed-handle central examples

These are not hard silicon limitations:

- failure to reach encrypted state at all
- inability to exchange SC P-256 data at all
- inability to run encrypted ATT after pairing at all

Those things already work to a meaningful extent.

## Suggested Acceptance Criteria for Calling BLE SC "Fixed"

At minimum:

1. Two-board SC handshake succeeds reproducibly.
2. Both boards report `encryption=ON`.
3. Central subscribes successfully.
4. Peripheral notifications arrive on the central.
5. Central writes arrive on the peripheral.
6. The duplex pair can run for at least 50 application exchanges without a
   supervision timeout.
7. Reconnect after disconnect does not regress into a dead or half-paired state.
8. `BlePairingEncryptionStatus` still works with the host-side regression
   runbook.

Stronger acceptance:

1. 20 repeated pair/encrypt/disconnect cycles on two boards
2. 10 bonded reconnect cycles
3. no new MIC failure signature in `BleEncryptionDebugCounters`
4. no central/peripheral pending-duplex deadlock signature at timeout

## If You Need a Minimal Starting Point

If you have almost no time:

1. Compile and flash `BlePairPeripheral` and `BlePairCentral`.
2. Watch serial on both.
3. Confirm `encryption=ON`.
4. Wait for:
   - `ble_pair central: notify=ping-N`
   - `ble_pair gatt-write val=ON/OFF`
5. If it later times out, immediately run:
   - `disc`
   - `enc`
   - `trace`
   on both monitors.
6. Start in [nrf54l15_hal_ble_central_event.inc](../hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_central_event.inc).

That is still the highest-signal path.
