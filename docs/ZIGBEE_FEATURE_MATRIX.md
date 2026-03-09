# Zigbee Feature Matrix

Last updated: 2026-03-09

This matrix separates the existing raw 802.15.4 capability from the Zigbee stack work needed for real interoperability with Home Assistant, ZHA, or Zigbee2MQTT.

## Interpretation

- `Present`: already in the repository and usable now.
- `Foundation`: implemented in-tree as codec or device-model building blocks, but not yet enough for a real joined Zigbee node.
- `Missing`: still required before the feature is real on-air behavior.

## Matrix

| Area | State | Notes |
|---|---|---|
| 802.15.4 channel select / TX / RX / CCA / ED | Present | `ZigbeeRadio` already drives the nRF54L15 RADIO block in 802.15.4 mode. |
| Raw short-address MAC data and MAC-command framing | Present | Current `ZigbeeRadio` helpers only cover a narrow MAC-lite subset. |
| MAC beacon / beacon-request / association-request / association-response / data-request codecs | Foundation | Clean generic MAC codec now supports short and extended address modes plus the key join and poll command frames. |
| MAC active scan / beacon parsing / automatic MAC ACK timing | Partial | Beacon build/parse, active-scan behavior, and a clean best-effort MAC ACK request/reply path now exist in the clean examples. Packet-capture validation against third-party coordinators is still missing. |
| MAC extended-address flows | Foundation | Generic MAC frame encode/decode now supports extended addressing, but runtime joining still is not wired. |
| NWK frame codec | Partial | Clean encode/decode now covers unsecured NWK plus demo-network secured NWK frames with auxiliary security headers and MIC-32 payload protection, along with NWK Rejoin Request/Response and End Device Timeout Request/Response command payloads. |
| NWK join state machine | Partial | `zigbee_commissioning` now owns shared end-device commissioning state, active-scan candidate selection, association wait handling, parent polling while waiting for Transport Key or Update Device, retry/timeout policy, MAC orphan notification plus coordinator realignment for retained-key parent recovery, NWK-secured rejoin request/response before the old reassociation fallback, explicit `rejoin_verify` and `leave_reset` phases, shared startup requests that choose secure rejoin when retained state allows it or fresh network steering otherwise, startup restore that resumes retained security state through secure rejoin instead of immediately reporting joined state, explicit startup network-steering requests instead of implicit joins from `kRestored`, and retained-key demo rejoin state transitions for the joinable HA examples, including BDB-style primary-then-secondary channel phases for both fresh steering and retained-network scan fallback when the first secure-rejoin attempt misses. Plain leave now stays in reset instead of immediately falling back into automatic fresh steering, and secure rejoin now stays in verification state until shared `Device Announce` plus End Device Timeout work completes, but the on-air behavior is still not full Zigbee 3.0 commissioning. |
| NWK routing / route discovery / route repair | Missing | Required for router/coordinator roles and multi-hop reliability. |
| NWK security headers / frame counters / nonce management | Partial | Clean security helpers now implement auxiliary security header encode/decode, nonce construction, AES-CCM* MIC-32 protection, persisted outgoing counters, and demo replay checks. Trust-center install and third-party interop remain open. |
| APS data frame codec | Partial | Added clean APS unicast plus group-addressed data encode/decode support, along with APS acknowledgement frame build/parse helpers for unicast HA and ZDO traffic. |
| APS command frame codec | Partial | Clean APS command encode/decode now covers generic command frames plus Transport Key, Update Device, and Switch Key commands. |
| APS acknowledgements / binding / group delivery | Partial | Clean binding-table handling, ZDO Bind/Unbind request/response support, demo-network APS group delivery, unicast APS ACK generation/consumption, bounded unicast retransmission, and duplicate suppression now exist for the clean coordinator and joinable HA endpoints. Broader APS retry coverage and real Zigbee multicast semantics remain open. |
| APS security / transport-key handling | Partial | The clean demo coordinator can now select a per-node preconfigured link key, including install-code-derived keys for the in-tree demo nodes, install the demo network key through APS-secured Transport Key, stage an alternate demo network key, refresh duplicate active or staged keys without resetting NWK replay state, reject stale or conflicting Transport Key sequence reuse, and follow retained-key demo rejoins with APS-secured `Update Device` plus APS-secured `Switch Key` rollout. The joinable HA examples now also require the correct coordinator short address, trust-center source, commissioning lifecycle state, and inbound APS counter progression for `Transport Key`, `Update Device`, and `Switch Key` acceptance, and they surface rejected trust-center command handling through explicit commissioning failure state instead of only timing out later. Full APS security coverage, trust-center lifecycle behavior, and third-party interop are still missing. |
| ZDO Node Descriptor request/response | Foundation | Response builders and client-side request construction now exist. |
| ZDO Power Descriptor request/response | Foundation | Response builders and client-side request construction now exist. |
| ZDO Active Endpoints request/response | Foundation | Response builders, request construction, and response parsing now exist. |
| ZDO Simple Descriptor request/response | Foundation | Response builders, request construction, and response parsing now exist. |
| ZDO Match Descriptor response | Foundation | Implemented for HA-style endpoint matching. |
| ZDO IEEE Address / Network Address request/response | Partial | Client request builders plus address-response parsing now exist, and the joinable HA endpoints answer IEEE/NWK-address requests for themselves. |
| ZDO Management Leave request/response | Partial | Codec support now exists, and the joinable HA endpoints accept a leave request, send the ZDO status response, use shared commissioning logic to decide plain leave vs retained-key secure rejoin, and otherwise clear persisted joined-state without immediately starting fresh steering again. |
| ZDO Management Permit Join request/response | Partial | Request build/parse helpers now exist, and the clean coordinator demo now advertises, enforces, and updates timed permit-join windows through beaconing, association behavior, and incoming ZDO Mgmt Permit Join requests. |
| Device Announce builder | Partial | Implemented, and the clean joinable HA examples now schedule it through the shared commissioning state machine after secured join activation, secure rejoin restoration, and joined-state restore. |
| BDB commissioning / network steering / rejoin | Partial | Core requirement for Home Assistant and Zigbee2MQTT interoperability. A shared commissioning state machine now exists in-tree for the joinable HA examples, including parent polling during trust-center wait states, retry/timeout handling, explicit shared requests for secure rejoin versus fresh network steering, startup restore that now resumes retained-state devices through secure rejoin instead of direct joined-state restore, explicit startup steering requests rather than implicit joins from any restored non-joined state, shared scheduling and retry throttling of `Device Announce` plus End Device Timeout requests after join or secure rejoin, explicit primary-then-secondary steering and retained-network scan phases, MAC orphan notification plus coordinator realignment, best-effort MAC ACK request/reply handling on the 802.15.4 path, NWK-secured rejoin request/response before the reassociation fallback, and fallback from exhausted secure-rejoin attempts back to fresh steering, but the current flow is still demo-network behavior rather than BDB-compliant Zigbee 3.0 commissioning. See `docs/ZIGBEE_3P0_PARITY_PLAN.md`. |
| Trust center default key / install-code support | Partial | Install-code CRC validation and install-code-derived link-key generation now exist, the joinable HA examples persist learned trust-center identity plus preconfigured-key mode, reject replayed or mis-sourced APS-secured `Update Device` and `Switch Key`, and the clean coordinator can stage alternate demo network keys followed by APS-secured Switch Key rollout on already joined nodes. Full trust-center lifecycle policy and standards-validated key updates are still missing. |
| Persistent network state (PAN/channel/addresses/keys/counters) | Partial | `zigbee_persistence.h/.cpp` now persists joined-state metadata, active and alternate demo network keys, outgoing NWK counters, incoming NWK secure-frame counters, inbound APS anti-replay counters, trust-center IEEE identity, preconfigured-key mode, reporting configuration, binding tables, and HA device state such as on/off and brightness level. |
| ZCL frame codec | Foundation | Added reusable ZCL frame encode/decode logic. |
| ZCL read-attributes request/response | Foundation | Client request build plus response build/parse now exist. |
| ZCL default responses | Foundation | Implemented for supported and unsupported commands. |
| ZCL configure-reporting request/response | Foundation | Client request build plus server response parsing/building now exist. |
| ZCL attribute-report parsing | Foundation | Clean parser now decodes report payloads for controller-side handling. |
| HA Basic cluster attributes | Foundation | Implemented for manufacturer/model/build/version/power-source reads. |
| HA On/Off cluster server | Foundation | Implemented read support plus `Off`, `On`, and `Toggle` commands. |
| HA Level Control cluster server | Foundation | Implemented read support plus `MoveToLevel`, `Move`, `Step`, `Stop`, and the `WithOnOff` command variants. |
| HA Power Configuration cluster server | Foundation | Implemented battery voltage and percentage reads. |
| HA Temperature Measurement cluster server | Foundation | Implemented measured/min/max/tolerance reads. |
| Identify cluster handling | Partial | Identify Time attribute reads plus `Identify`, `Identify Query`, and `Trigger Effect` acceptance are now implemented; timed identify countdown behavior is still example-driven. |
| Reporting configuration and periodic reports | Partial | ZCL configure-reporting parsing, report generation, and scheduling are implemented for supported on/off, level, temperature, and battery attributes; full secure joined-network behavior is still missing. |
| Groups cluster behavior | Partial | Clean server-side group table plus Add/View/Membership/Remove flows are now implemented for HA light endpoints, and the demo coordinator can enroll lights into a shared test group. |
| Scenes cluster behavior | Partial | Clean scene table plus Add/View/Store/Recall/Membership flows are now implemented for HA light endpoints, including On/Off and Level snapshots. |
| OTA Upgrade cluster behavior | Missing | Not implemented. |
| Home Assistant generic On/Off Light profile | Partial | Static and joinable clean examples now exist; the joinable example now uses the shared `zigbee_commissioning` state machine for scan, association, Transport Key install, retained-key demo rejoin, trust-center wait-state polling, shared `Device Announce` plus End Device Timeout scheduling, retry/failure handling, configurable channel masks, bounded unicast APS retransmission with duplicate suppression, and unicast APS ACK handling, while also answering ZDO/ZCL including IEEE/NWK-address and leave requests, learning or pinning trust-center identity, enforcing encrypted Transport Key delivery by default, accepting APS group-addressed commands, and emitting reports on the clean demo network. |
| Home Assistant generic Dimmable Light profile | Partial | Static and joinable clean examples now exist; the joinable example now uses the shared `zigbee_commissioning` state machine for scan, association, Transport Key install, retained-key demo rejoin, trust-center wait-state polling, shared `Device Announce` plus End Device Timeout scheduling, retry/failure handling, configurable channel masks, bounded unicast APS retransmission with duplicate suppression, and unicast APS ACK handling, while also providing HA Level Control behavior, brightness persistence, PWM-backed LED output, learned or pinned trust-center identity, encrypted Transport Key enforcement, and APS group-addressed command handling on the clean demo network. |
| Home Assistant generic Temperature Sensor profile | Partial | Static and joinable clean examples now exist; the joinable example now uses the shared `zigbee_commissioning` state machine for scan, association, Transport Key install, retained-key demo rejoin, trust-center wait-state polling, shared `Device Announce` plus End Device Timeout scheduling, retry/failure handling, configurable channel masks, bounded unicast APS retransmission with duplicate suppression, and unicast APS ACK handling, while also answering ZDO/ZCL including IEEE/NWK-address and leave requests, learning or pinning trust-center identity, enforcing encrypted Transport Key delivery by default, and emitting temperature/power reports on the clean demo network. |
| Real joined end device on an existing coordinator | Partial | Clean joinable On/Off Light, Dimmable Light, and Temperature Sensor examples now work against the in-tree clean coordinator demo, no longer hard-code that trust center into their security checks, can recover retained-key parent loss through orphan notification plus coordinator realignment and NWK-secured rejoin request/response before reassociation fallback, can negotiate End Device Timeout on the joined demo network, can retry secure rejoin by scanning for the retained network across configured channel masks, and now expose preferred EPID plus local identity/install-code overrides as build-time policy knobs instead of requiring sketch edits, but third-party coordinator interoperability is still missing. |
| Router role | Missing | Requires MAC+NWK routing, security, and persistence. |
| Coordinator role | Partial | Clean coordinator demo now performs beaconing, timed permit-join enforcement, association, address allocation, queued delivery over MAC data-request polling, unicast APS ACK handling with bounded retransmission, ZDO/ZCL discovery, ZDO binding setup, reporting setup, demo leave requests, orphan notification handling plus coordinator realignment for known retained-key nodes, NWK-secured rejoin response plus End Device Timeout response handling, retained-key demo rejoin handling via APS-secured Update Device, and a polled alternate-network-key rollout via APS-secured Transport Key plus Switch Key, along with demo On/Off and Level Control commands for joined HA lights and temperature sensors, including a shared light-control group. It now also tracks per-node trust-center lifecycle state and only delivers `Transport Key`, `Update Device`, and `Switch Key` through the expected demo secured path for that state. Trust-center/security/routing remain open. |
| Zigbee2MQTT serial adapter / NCP compatibility | Missing | Needed only if this board should act as a coordinator directly under Z2M. |

## What Is Actually Supported Now

Today the repo can:

- Use the nRF54L15 radio in 802.15.4 mode.
- Send and receive raw 802.15.4 frames on a fixed channel/PAN.
- Build and parse generic MAC, NWK, APS, and ZCL frames locally.
- Build and parse demo-network secured NWK frames with clean AES-CCM* helpers, auxiliary security headers, MIC validation, and NWK rejoin or End Device Timeout command payloads.
- Build and parse APS unicast, acknowledgement, and group-addressed HA traffic locally.
- Build and parse APS command frames, including Transport Key, Update Device, and Switch Key payloads, plus their APS-secured trust-center variants.
- Protect demo APS Transport Key delivery with either an install-code-derived link key or the ZigBeeAlliance09 fallback key, depending on node policy.
- Treat duplicate or stale APS `Transport Key` commands as refresh-or-reject events instead of always resetting joined key state.
- Build and parse ZDO IEEE-address, NWK-address, management-leave, and management permit-join request payloads plus address responses.
- Perform clean active scan plus beacon parsing on the in-tree demo network.
- Request and reply to MAC acknowledgements for unicast 802.15.4 data and command frames on the clean Zigbee radio path.
- Reuse a shared end-device commissioning state machine for scan, association, Transport Key install, parent polling during trust-center wait states, retry/timeout handling, retained-network masked-channel fallback scanning, MAC orphan recovery, `Device Announce` plus End Device Timeout negotiation, and retained-key demo rejoin across the joinable HA examples.
- Persist Zigbee-oriented network/reporting/binding state, active and alternate demo security material, trust-center identity, preconfigured-key provenance, and secure-frame counters with a clean Preferences-backed store.
- Build Home Assistant-oriented descriptors and standard cluster responses for:
  - an On/Off Light
  - a Dimmable Light
  - a Temperature Sensor
- Maintain Identify, Groups, and a clean Scenes subset for HA light endpoints.
- Accept APS group-addressed On/Off and Level Control commands on HA light examples once they join a configured group.
- Exchange unicast APS ACK frames between the clean coordinator and joinable HA endpoints for ZDO and HA application traffic, with bounded retransmission and duplicate suppression on the clean demos.
- Answer ZDO IEEE-address and NWK-address requests on the joinable HA endpoint examples.
- Accept management leave requests on the joinable HA endpoint examples, using shared commissioning logic to hold a plain leave in explicit reset state instead of immediately auto-steering again or to transition into retained-key secure rejoin when the leave request sets the rejoin flag.
- Reuse persisted demo network keys, trust-center identity, key provenance, and counters so the joinable HA endpoint examples come back through retained-key secure rejoin after restart instead of assuming they are already joined.
- Run static-network HA examples for:
  - an On/Off Light
  - a Dimmable Light
  - a Temperature Sensor
- Run clean joinable HA light, dimmable-light, and temperature-sensor examples that associate, install a demo network key through APS-secured Transport Key commands using either install-code-derived or ZigBeeAlliance09 preconfigured keys, keep polling while waiting for trust-center follow-up, emit `Device Announce` after secured join activation or secure rejoin restoration, negotiate End Device Timeout, serve ZDO/ZCL, reject replayed secured NWK frames from the demo coordinator, persist inbound APS counters plus active and alternate network keys, accept `Transport Key`, `Update Device`, and `Switch Key` only from the expected trust-center source and lifecycle state, allow preferred EPID plus local identity/install-code overrides through build-time policy macros, retry retained-key secure rejoin by attempting orphan recovery and NWK-secured rejoin before scanning for the retained network across configured channel masks, exchange unicast APS ACKs with bounded retransmission and duplicate suppression, and report on the clean coordinator demo network.
- Run a clean coordinator demo that beacons, advertises and enforces timed permit-join windows, accepts association, allocates short addresses, delivers a demo network key through APS-secured Transport Key commands using per-node preconfigured link-key policy, discovers descriptors, installs bindings, configures reporting, transitions nodes from plaintext post-join traffic to secured NWK traffic, can queue management-leave requests, answers orphan notification with coordinator realignment for retained-key demo rejoin handling, answers NWK-secured rejoin and End Device Timeout requests, emits APS-secured `Update Device` for the secure-rejoin follow-up, can stage and switch an alternate demo network key on already joined children, tracks per-node trust-center lifecycle state for those commands, enrolls demo light groups, exchanges unicast APS ACKs with bounded retransmission, and queues HA traffic for polled delivery, including brightness control for dimmable lights.

Today the repo cannot yet:

- join a third-party Zigbee 3.0 coordinator,
- interoperate with a third-party trust center's APS-secured transport-key exchange,
- negotiate trust-center security,
- complete a Zigbee 3.0-compliant secure joined-state lifecycle,
- act as a coordinator that Zigbee2MQTT can use as an adapter.

## Minimum Remaining Work For Home Assistant Device Interop

1. Replace the demo-only join path with fully validated third-party coordinator MAC timing and Zigbee 3.0 commissioning behavior.
2. Extend the current preconfigured-link-key APS Transport Key path into full Zigbee 3.0 trust-center commissioning behavior.
3. Extend the new joined end-device flow from the clean demo coordinator to third-party coordinators used by ZHA or Zigbee2MQTT.
4. Extend the existing NWK security persistence and replay checks into full secure joined-state behavior across rejoin, APS security, and trust-center key updates, including broader APS retransmission coverage beyond the current bounded unicast demo policy.

Execution planning for the first three blocker areas is tracked in `docs/ZIGBEE_3P0_PARITY_PLAN.md`, and the coordinator-facing packet flow expected for future ZHA/Zigbee2MQTT bring-up is tracked in `docs/ZIGBEE_EXTERNAL_COORDINATOR_FLOW.md`.

## Minimum Remaining Work For Zigbee2MQTT Coordinator Interop

If the goal is to use this board as the coordinator behind Zigbee2MQTT, the scope is larger:

1. Complete coordinator-grade MAC/NWK/APS/security behavior.
2. Add a stable host-facing serial protocol that Zigbee2MQTT already understands, or add a new adapter implementation upstream.
3. Extend the current demo permit-join, leave, and child/address handling into the management behavior expected of a real coordinator, then add routing.

That should be treated as a separate phase after end-device interoperability is real.
