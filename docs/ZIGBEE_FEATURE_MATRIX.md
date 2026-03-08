# Zigbee Feature Matrix

Last updated: 2026-03-08

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
| MAC active scan / beacon parsing / automatic MAC ACK timing | Partial | Beacon build/parse and active-scan behavior now exist in the clean examples, but interoperable MAC ACK timing is still missing. |
| MAC extended-address flows | Foundation | Generic MAC frame encode/decode now supports extended addressing, but runtime joining still is not wired. |
| NWK frame codec | Partial | Clean encode/decode now covers unsecured NWK plus demo-network secured NWK frames with auxiliary security headers and MIC-32 payload protection. |
| NWK join state machine | Partial | Clean examples now carry a single-parent join flow from MAC association into NWK/APS/ZDO traffic, including demo secure NWK operation after join, but not Zigbee 3.0 commissioning. |
| NWK routing / route discovery / route repair | Missing | Required for router/coordinator roles and multi-hop reliability. |
| NWK security headers / frame counters / nonce management | Partial | Clean security helpers now implement auxiliary security header encode/decode, nonce construction, AES-CCM* MIC-32 protection, persisted outgoing counters, and demo replay checks. Trust-center install and third-party interop remain open. |
| APS data frame codec | Partial | Added clean APS unicast plus group-addressed data encode/decode support; APS ACK behavior is still missing. |
| APS acknowledgements / binding / group delivery | Partial | Clean binding-table handling, ZDO Bind/Unbind request/response support, and demo-network APS group delivery now exist for HA lights. APS ACK behavior and real Zigbee multicast semantics remain open. |
| APS security / transport-key handling | Missing | Trust-center link-key transport, APS command security, and network-key install flow are still absent. |
| ZDO Node Descriptor request/response | Foundation | Response builders and client-side request construction now exist. |
| ZDO Power Descriptor request/response | Foundation | Response builders and client-side request construction now exist. |
| ZDO Active Endpoints request/response | Foundation | Response builders, request construction, and response parsing now exist. |
| ZDO Simple Descriptor request/response | Foundation | Response builders, request construction, and response parsing now exist. |
| ZDO Match Descriptor response | Foundation | Implemented for HA-style endpoint matching. |
| Device Announce builder | Partial | Implemented and now used by the clean joinable HA light example after association completes. |
| BDB commissioning / network steering / rejoin | Missing | Core requirement for Home Assistant and Zigbee2MQTT interoperability. |
| Trust center default key / install-code support | Missing | Required for secure Zigbee 3.0 joining. |
| Persistent network state (PAN/channel/addresses/keys/counters) | Partial | `zigbee_persistence.h/.cpp` now persists joined-state metadata, demo network keys, outgoing NWK counters, incoming secure-frame counters, reporting configuration, binding tables, and HA device state such as on/off and brightness level. |
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
| Home Assistant generic On/Off Light profile | Partial | Static and joinable clean examples now exist; the joinable example can scan, associate, announce, answer ZDO/ZCL, poll for queued coordinator traffic, accept APS group-addressed commands, and emit reports on the clean demo network. |
| Home Assistant generic Dimmable Light profile | Partial | Static and joinable clean examples now exist; they add HA Level Control behavior, brightness persistence, PWM-backed LED output, and APS group-addressed command handling on the clean demo network. |
| Home Assistant generic Temperature Sensor profile | Partial | Static and joinable clean examples now exist; the joinable example can scan, associate, announce, answer ZDO/ZCL, and emit temperature/power reports on the clean demo network. |
| Real joined end device on an existing coordinator | Partial | Clean joinable On/Off Light, Dimmable Light, and Temperature Sensor examples now work against the in-tree clean coordinator demo; third-party coordinator interoperability is still missing. |
| Router role | Missing | Requires MAC+NWK routing, security, and persistence. |
| Coordinator role | Partial | Clean coordinator demo now performs beaconing, association, address allocation, queued delivery over MAC data-request polling, ZDO/ZCL discovery, ZDO binding setup, reporting setup, and demo On/Off plus Level Control commands for joined HA lights and temperature sensors, including a shared light-control group. Trust-center/security/routing remain open. |
| Zigbee2MQTT serial adapter / NCP compatibility | Missing | Needed only if this board should act as a coordinator directly under Z2M. |

## What Is Actually Supported Now

Today the repo can:

- Use the nRF54L15 radio in 802.15.4 mode.
- Send and receive raw 802.15.4 frames on a fixed channel/PAN.
- Build and parse generic MAC, NWK, APS, and ZCL frames locally.
- Build and parse demo-network secured NWK frames with clean AES-CCM* helpers, auxiliary security headers, and MIC validation.
- Build and parse APS unicast plus group-addressed HA traffic locally.
- Perform clean active scan plus beacon parsing on the in-tree demo network.
- Persist Zigbee-oriented network/reporting/binding state, demo security material, and secure-frame counters with a clean Preferences-backed store.
- Build Home Assistant-oriented descriptors and standard cluster responses for:
  - an On/Off Light
  - a Dimmable Light
  - a Temperature Sensor
- Maintain Identify, Groups, and a clean Scenes subset for HA light endpoints.
- Accept APS group-addressed On/Off and Level Control commands on HA light examples once they join a configured group.
- Run static-network HA examples for:
  - an On/Off Light
  - a Dimmable Light
  - a Temperature Sensor
- Run clean joinable HA light, dimmable-light, and temperature-sensor examples that associate, announce, poll, serve ZDO/ZCL, reject replayed secured NWK frames from the demo coordinator, and report on the clean coordinator demo network.
- Run a clean coordinator demo that beacons, accepts association, allocates short addresses, discovers descriptors, installs bindings, configures reporting, transitions nodes from plaintext post-join traffic to secured NWK traffic, enrolls demo light groups, and queues HA traffic for polled delivery, including brightness control for dimmable lights.

Today the repo cannot yet:

- join a third-party Zigbee 3.0 coordinator,
- receive or install a trust-center network key over APS transport-key exchange,
- negotiate trust-center security,
- complete a secure joined-state lifecycle,
- act as a coordinator that Zigbee2MQTT can use as an adapter.

## Minimum Remaining Work For Home Assistant Device Interop

1. Replace the demo-only join path with interoperable MAC timing and Zigbee 3.0 commissioning behavior.
2. Implement Zigbee 3.0 commissioning essentials: BDB steering, trust-center link key handling, APS transport-key decode, and network-key install.
3. Extend the new joined end-device flow from the clean demo coordinator to third-party coordinators used by ZHA or Zigbee2MQTT.
4. Extend the existing NWK security persistence and replay checks into full secure joined-state behavior across rejoin, APS security, and trust-center key updates.

## Minimum Remaining Work For Zigbee2MQTT Coordinator Interop

If the goal is to use this board as the coordinator behind Zigbee2MQTT, the scope is larger:

1. Complete coordinator-grade MAC/NWK/APS/security behavior.
2. Add a stable host-facing serial protocol that Zigbee2MQTT already understands, or add a new adapter implementation upstream.
3. Implement management, permit-join, child/address handling, and routing behavior expected of a real coordinator.

That should be treated as a separate phase after end-device interoperability is real.
