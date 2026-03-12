# Zigbee Home Assistant Bring-Up

Last updated: 2026-03-12

This is the repo-local bring-up recipe for the first real Home Assistant / ZHA test. It is deliberately narrow: one end-device example, one coordinator, one known channel, and conservative commissioning timing.

## Recommended First Test

Use:

- `ZigbeeHaOnOffLightJoinable`
- Home Assistant with ZHA
- one fixed Zigbee channel
- install-code join if the coordinator is configured for it, otherwise fallback to the well-known key only if needed for the first proof-of-life

The on/off light is the cleanest first probe because it has the smallest HA surface and the least sketch-side behavior on top of the shared commissioning stack.

## Build-Time Settings

Start with a narrow, explicit configuration instead of the repo defaults that try to stay broad and demo-friendly.

Recommended overrides:

- `NRF54L15_CLEAN_ZIGBEE_PRIMARY_CHANNEL_MASK`
  Set this to only the Home Assistant coordinator channel.
- `NRF54L15_CLEAN_ZIGBEE_SECONDARY_CHANNEL_MASK=0`
- `NRF54L15_CLEAN_ZIGBEE_PREFERRED_EXTENDED_PAN_ID`
  Leave `0` for the very first join attempt, or pin it after the network is known.
- `NRF54L15_CLEAN_ZIGBEE_TRUST_CENTER_IEEE`
  Leave `0` to learn on the first secure install, or pin it if you already know the coordinator IEEE.
- `NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE`
  `1` if you want install-code-derived preconfigured key mode.
- `NRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY`
  `0` for stricter Zigbee 3.0-style testing when the coordinator is configured for install code, otherwise `1` for the first interop probe.
- `NRF54L15_CLEAN_ZIGBEE_REQUIRE_ENCRYPTED_TRANSPORT_KEY=1`

Use the conservative timing knobs for third-party coordinator bring-up:

- `NRF54L15_CLEAN_ZIGBEE_ACTIVE_SCAN_WINDOW_MS=120`
- `NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_RESPONSE_TIMEOUT_MS=4000`
- `NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_LISTEN_MS=120`
- `NRF54L15_CLEAN_ZIGBEE_ASSOCIATION_POLL_RETRY_DELAY_MS=40`
- `NRF54L15_CLEAN_ZIGBEE_COORDINATOR_REALIGNMENT_TIMEOUT_MS=400`
- `NRF54L15_CLEAN_ZIGBEE_NWK_REJOIN_RESPONSE_TIMEOUT_MS=1500`

## What "Good" Looks Like

For a successful first join, the serial log should show this shape:

1. active scan finds the target network on the expected channel
2. association succeeds and a non-temporary short address is assigned
3. APS-secured `Transport Key` is accepted
4. joined state becomes active
5. `Device Announce` is emitted
6. `End Device Timeout Request` is sent and a valid response is accepted
7. ZHA interviews the device with Node Descriptor, Power Descriptor, Active EP, Simple Descriptor, and Basic-cluster reads

If join succeeds but ZHA stalls later, the remaining problem is probably not commissioning anymore. It is more likely a ZDO/ZCL interview detail.

The in-tree `ZigbeeHaCoordinatorJoinDemo` now mirrors that same early interview order, so it is a better local pre-ZHA check than it was before.

## What To Test First

1. Put ZHA into permit-join mode on a known fixed channel.
2. Build `ZigbeeHaOnOffLightJoinable` with the narrow channel mask and the timing overrides above.
3. Watch serial during join and keep a packet capture if possible.
4. If the first join works, repeat with:
   - pinned `TRUST_CENTER_IEEE`
   - pinned `PREFERRED_EXTENDED_PAN_ID`
   - `ALLOW_WELL_KNOWN_LINK_KEY=0` when install-code join is configured
5. Only after the on/off light is stable, move to:
   - `ZigbeeHaDimmableLightJoinable`
   - `ZigbeeHaTemperatureSensorJoinable`

## What Is Still Expected To Be Rough

- true BDB-compliant external-coordinator behavior is still not claimed
- trust-center key-update behavior is still only partially validated
- retained-key rejoin against third-party coordinators is still a later test after first join works
- packet-capture validation is still needed before calling ZHA interop "done"
