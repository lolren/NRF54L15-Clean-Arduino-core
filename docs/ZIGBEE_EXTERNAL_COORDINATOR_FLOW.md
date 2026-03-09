# Zigbee External Coordinator Flow

Last updated: 2026-03-09

This document captures the packet-level behavior the clean joinable HA examples now expect from a future third-party coordinator bring-up. It is not a claim that ZHA or Zigbee2MQTT interoperability already works.

## Scope

These notes apply to the clean end-device examples:

- `ZigbeeHaOnOffLightJoinable`
- `ZigbeeHaDimmableLightJoinable`
- `ZigbeeHaTemperatureSensorJoinable`

They describe the current in-tree behavior that must line up with a future external coordinator or NCP integration.

## Join And Install Flow

Expected sequence:

1. Coordinator advertises a permit-join-capable beacon on a selected channel.
2. End device performs active scan and ranks beacon candidates through `zigbee_commissioning`.
3. End device sends MAC association request and polls for MAC association response.
4. Coordinator allocates a short address and returns the association response on the next poll.
5. Coordinator delivers APS `Transport Key` with the network key.
6. The `Transport Key` is expected to be APS encrypted by default.
7. The end device accepts either:
   - an install-code-derived preconfigured link key
   - the ZigBeeAlliance09 fallback key when policy allows it
8. End device installs the network key, persists trust-center identity and APS anti-replay state, then emits `Device Announce`.

Current clean examples do not implement BDB steering or install-code discovery on-air. They only implement the device-side acceptance path.

## Secure Rejoin Flow

Expected sequence:

1. End device restores retained PAN, parent, trust-center IEEE, link-key provenance, and active network key.
2. End device performs a retained-key reassociation attempt on the retained channel and parent.
3. If that misses, the device scans the configured primary and secondary masks for a beacon matching the retained PAN or Extended PAN ID, then retries reassociation on the best retained-network candidate it finds.
4. Coordinator recognizes the node as known and accepts the reassociation.
5. Coordinator delivers NWK-secured APS `Update Device` from the expected trust-center source.
6. End device accepts `Update Device` only while it is in the secure-rejoin wait state, restores joined state, and resumes secured polling and reporting.

Current clean examples require retained key material before they will attempt secure rejoin. This is still not BDB rejoin.

## Network Key Update Flow

The clean demo stack now supports a staged alternate network key.

Expected sequence:

1. Coordinator keeps the current active network key and key sequence in use for normal secured NWK traffic.
2. Coordinator delivers a new APS-secured `Transport Key` carrying an alternate network key and a higher sequence number.
3. End device validates the APS security context and stores the new key as `alternate`, without activating it immediately.
4. Coordinator later sends APS `Switch Key` over NWK security using the still-active network key.
5. End device promotes the alternate key to active, clears the staged alternate slot, resets NWK replay state, and resumes secured traffic on the new sequence.

This is a demo rollout path for the in-tree coordinator. It is not yet validated against a third-party Trust Center.

## Current Device-Side Policy

Relevant compile-time knobs already present in the joinable examples:

- `NRF54L15_CLEAN_ZIGBEE_PRIMARY_CHANNEL_MASK`
- `NRF54L15_CLEAN_ZIGBEE_SECONDARY_CHANNEL_MASK`
- `NRF54L15_CLEAN_ZIGBEE_USE_INSTALL_CODE`
- `NRF54L15_CLEAN_ZIGBEE_ALLOW_WELL_KNOWN_LINK_KEY`
- `NRF54L15_CLEAN_ZIGBEE_REQUIRE_ENCRYPTED_TRANSPORT_KEY`
- `NRF54L15_CLEAN_ZIGBEE_TRUST_CENTER_IEEE`

Behavior:

- Primary and secondary scan masks can be narrowed for external-coordinator bring-up instead of hard-coding a full-channel demo scan.
- Trust-center IEEE may be pinned or learned on first secure install.
- Well-known link-key fallback is optional.
- Encrypted `Transport Key` is required by default.
- Secure rejoin depends on retained key material and link-key provenance.
- `Update Device` and `Switch Key` are only accepted from the expected trust-center source, and only in the right device lifecycle state.

## What An External Coordinator Must Match

Minimum behavioral match for future ZHA/Zigbee2MQTT interop work:

- Beacon and association timing that the current MAC polling path can interoperate with.
- APS-encrypted `Transport Key` delivery with the expected preconfigured link key.
- NWK-secured `Update Device` for retained-key rejoin handling, sourced from the coordinator short address and expected trust-center IEEE.
- NWK-secured APS `Switch Key` if network-key update is attempted, again sourced from the expected trust center.
- Home Automation descriptor discovery, reporting configuration, and standard ZDO responses.

## What Still Blocks Real Interop

- Interoperable MAC ACK timing.
- Real BDB steering, startup, and rejoin behavior.
- Trust-center key-update lifecycle validation against a third-party coordinator.
- Full APS retransmission policy beyond the current single ACK exchange.
- Packet-capture validation against ZHA or Zigbee2MQTT on real hardware.
