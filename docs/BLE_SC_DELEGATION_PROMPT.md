# Prompt For Another AI: Continue BLE Secure Connections Work

Use this prompt as a starting point with another AI.

It is intentionally explicit and opinionated. The goal is to save tokens and
avoid another broad exploratory pass.

---

You are working in the `nrf54-arduino-core` repository on a custom BLE
controller for the nRF54L15.

Your task is to continue BLE LE Secure Connections debugging and implementation.

Read this first:

- `docs/BLE_SC_HANDOFF.md`
- `docs/BLE_REGRESSION_RUNBOOK.md`

Then inspect the current BLE SC code paths in:

- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal.h`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_ll_security.inc`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_att_l2cap.inc`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_central_event.inc`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_rx.inc`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tx.inc`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/nrf54l15_hal_parts/nrf54l15_hal_ble_peripheral_event_tail.inc`
- `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/src/matter_secp256r1.cpp`

Use these examples as the main two-board secure-duplex harness:

- `.../examples/BLE/Security/BlePairPeripheral/BlePairPeripheral.ino`
- `.../examples/BLE/Security/BlePairCentral/BlePairCentral.ino`

Use these examples as secondary host-facing regression probes:

- `.../examples/BLE/Security/BlePairingEncryptionStatus/BlePairingEncryptionStatus.ino`
- `.../examples/BLE/Security/BleBondPersistenceProbe/BleBondPersistenceProbe.ino`

## What is already true

Do not start from the assumption that LE Secure Connections is absent.

The current controller can already:

- exchange SC pairing messages
- do software P-256 key generation and ECDH
- reach encrypted link state
- perform encrypted subscribe / notify / write operations
- complete multiple successful duplex application cycles after encryption

The unresolved bug is later in the controller lifecycle, not at the first
moment of SMP negotiation.

## The current bug

The remaining bug is an intermittent supervision timeout after successful SC
handshake and after some amount of encrypted ATT duplex traffic.

Historically observed failure signatures:

- handshake-adjacent failures around `LL_START_ENC_REQ` / `LL_START_ENC_RSP`
- later failures after subscribe / write response
- later steady-state failures where:
  - central had pending encrypted ATT write
  - peripheral had pending encrypted ATT notification
  - both sides believed the peer had progressed
  - the link still timed out

Most suspicious area:

- event-to-event carry-over and queued encrypted ATT traffic progression,
  especially on the central side

## Your goal

Your goal is not just to make `encryption=ON` appear once.

Your goal is to make the secure duplex path stable.

Minimum acceptance:

1. Two-board SC handshake succeeds reliably.
2. Central subscribes.
3. Peripheral notifications arrive at the central.
4. Central writes arrive at the peripheral.
5. This continues for at least 50 application exchanges without supervision
   timeout.

Stronger acceptance:

1. 20 repeated secure sessions
2. 10 bonded reconnects
3. no new MIC-failure regressions

## How to work

1. Reproduce using the two-board harness first.
2. Keep changes small and local.
3. Add instrumentation where needed, but remove or gate spam before finishing.
4. Do not assume a phone or BlueZ trace tells the full story when the two-board
   harness already proves more.
5. Focus on real state transitions, not vague timing guesses.

## What not to do

- Do not restart from legacy-only pairing fallback logic.
- Do not throw away the current SC path and replace it with a broad rewrite.
- Do not assume the main problem is still P-256 speed.
- Do not use `clear` casually during reconnect testing because it clears bonds.
- Do not treat the central example as a generic client; it uses fixed peer
  handles for the matching peripheral example.
- Do not enable `BLE_PAIR_CENTRAL_WRITE_WITH_RESPONSE=1` unless you also change
  the characteristic contract.

## Suggested initial plan

1. Rebuild and flash:
   - `BlePairPeripheral`
   - `BlePairCentral`
2. Verify:
   - connect
   - `encryption=ON`
   - `subscribed`
   - `notify=ping-1`
   - `gatt-write val=ON/OFF`
3. Let it soak until failure.
4. Immediately capture on both sides:
   - `enc`
   - `disc`
   - `trace`
5. Compare:
   - pending TX state
   - last TX/RX opcodes
   - whether both sides still had encrypted app traffic queued
6. Instrument the exact decision points for:
   - promoting queued TX
   - sending empty ACK instead
   - inferring peer ACK
   - preserving or dropping fresh TX permission
   - entering the next event with pending TX

## Concrete hypotheses worth testing

1. The central side is still too conservative about promoting a queued request
   after an earlier ACK-only event.
2. The central/peripheral pair can both end up with valid pending encrypted
   application data and no side willing to spend the next event correctly.
3. One of the implicit ACK heuristics is technically wrong in a later duplex
   pattern even though it helped earlier cases.
4. Background connection servicing interacts with explicit poll timing in a way
   that only appears after encryption and queue buildup.

## Useful commands

Sketchbook override:

```bash
mkdir -p /tmp/arduino-user/hardware
rm -rf /tmp/arduino-user/hardware/nrf54l15clean
ln -s "$PWD/hardware/nrf54l15clean" /tmp/arduino-user/hardware/nrf54l15clean
```

Temporary config:

```bash
tmpcfg=$(mktemp)
cat > "$tmpcfg" <<'YAML'
directories:
  user: /tmp/arduino-user
  data: /home/lolren/.arduino15
YAML
```

Compile peripheral:

```bash
arduino-cli compile \
  --config-file "$tmpcfg" \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral
```

Compile central:

```bash
arduino-cli compile \
  --config-file "$tmpcfg" \
  --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral
```

Upload:

```bash
arduino-cli upload --config-file "$tmpcfg" -p /dev/ttyACM0 --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairPeripheral

arduino-cli upload --config-file "$tmpcfg" -p /dev/ttyACM1 --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 \
  hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/BLE/Security/BlePairCentral
```

Port discovery:

```bash
arduino-cli board list
python3 - <<'PY'
from serial.tools import list_ports
for p in list_ports.comports():
    print(p.device, p.serial_number, p.description)
PY
```

## Working assumptions you should challenge only if evidence forces it

- The controller can already do enough ECC to complete SC.
- The remaining bug is likely in controller event progression and queued
  encrypted traffic arbitration.
- The two-board harness is the highest-signal reproducer.
- The central event path is the most suspicious current seam.

## Deliverable expectation

If you fix something, prove it with:

- compile success
- hardware flashing
- serial evidence from both boards
- at least a modest soak, not a single happy-path cycle

If you do not fix it, leave behind:

- the narrowest reproduced failure
- exact logs
- exact file/branch of logic responsible
- the next best hypothesis

Do not leave broad guesses with no captured state.

---

If you need a single sentence summary:

The core already has real LE Secure Connections and real encrypted ATT traffic,
but it still deadlocks or times out later in secure duplex traffic, probably in
central-side event progression and queued encrypted ATT carry-over.
