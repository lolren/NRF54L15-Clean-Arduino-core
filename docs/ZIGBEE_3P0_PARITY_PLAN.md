# Zigbee 3.0 Parity Plan

Last updated: 2026-03-12

This is the repo-local parity plan for the first three remaining Zigbee 3.0 blockers. It follows the `gsd-plan-phase` and `gsd-execute-phase` workflow shape, but the repo does not currently contain a full GSD project scaffold such as `PROJECT.md` or `ROADMAP.md`, so the plan is tracked here instead of under `.planning/phases/`.

## Goal

Close the first three blocker areas in one coordinated execution batch:

1. BDB-style commissioning, network steering, and rejoin.
2. Trust-center lifecycle, including install-code and key-update behavior.
3. Third-party coordinator interoperability prep for ZHA and Zigbee2MQTT-backed networks.

## Current Baseline

The in-tree clean Zigbee demo stack already has:

- reusable MAC/NWK/APS/ZDO/ZCL codecs,
- a clean best-effort MAC ACK request/reply path for unicast 802.15.4 data and command traffic,
- a shared `zigbee_commissioning` end-device state machine for the joinable HA examples,
- demo secured NWK transport with persisted counters,
- APS-secured Transport Key delivery,
- install-code CRC validation and install-code-derived link-key generation,
- persisted trust-center IEEE identity plus inbound APS anti-replay state,
- demo coordinator and joinable HA examples with secured Transport Key install,
- secured Update Device delivery for the retained-key demo rejoin path,
- alternate demo network-key persistence plus Switch Key codec support,
- a polled demo network-key update rollout on the clean coordinator,
- duplicate/stale APS `Transport Key` hardening so joined nodes no longer treat same-sequence key reuse as a fresh activation,
- a coordinator-facing flow document for future external-coordinator bring-up.

That is useful foundation work, but it is still not real Zigbee 3.0 parity.

## Execution Waves

### Wave 1: Shared Commissioning State Machine

Objective: replace the sketch-local join flow with a shared commissioning engine that can drive first join, steering retries, and retained-key rejoin without hard-coded demo assumptions.

Status: partially landed. The joinable HA examples now use shared commissioning state, active scan, association, parent polling while waiting for Transport Key or Update Device, retry/timeout handling, shared scheduling of `Device Announce` plus End Device Timeout requests after join or secure rejoin, BDB-style primary-then-secondary channel steering for fresh join and retained-network scan fallback, explicit shared startup requests for secure rejoin versus fresh network steering, startup restore that resumes retained security state through secure rejoin instead of claiming immediate joined-state success, and explicit startup network-steering requests instead of treating any `kRestored` state as an automatic join trigger. MAC orphan-notification plus coordinator-realignment recovery, NWK-secured rejoin request/response before the reassociation fallback, retained-key rejoin transitions through `zigbee_commissioning`, shared accepted-`Mgmt Leave` disposition handling, and explicit `rejoin_verify` / `leave_reset` commissioning states so plain leave no longer falls straight back into automatic fresh steering and secure rejoin does not report fully joined until the shared post-rejoin verification work clears. The behavior is still demo-network commissioning rather than BDB.

Deliverables:

- Promote `zigbee_commissioning` from policy helper to a reusable commissioning state machine.
- Move active-scan candidate ranking, permit-join filtering, and parent selection logic out of the joinable sketches.
- Add explicit commissioning states for idle, scan, association, transport-key wait, joined, rejoin pending, rejoin verify, and leave/reset.
- Track failure reasons, retry budgets, and trust-center wait-state polling so join/rejoin does not silently collapse into a demo-only fallback.
- Expose a small reusable API that the HA joinable examples can call instead of duplicating association/rejoin rules.

Verification:

- Self-test coverage for beacon scoring, policy checks, and commissioning-state transitions.
- Compile the coordinator demo and all joinable HA examples.
- Manual two-board run: first join, power-cycle restore, leave, and rejoin against the in-tree coordinator.

### Wave 2: Trust-Center Lifecycle And Security Hardening

Objective: move from a single demo Transport Key exchange toward a real trust-center lifecycle with durable identity and key-management policy.

Status: partially landed. Install-code-derived and ZigBeeAlliance09 link-key policy, persisted trust-center identity, encrypted Transport Key enforcement, retained-key rejoin gating, alternate network-key persistence, APS-secured Update Device and Switch Key handling with inbound APS anti-replay checks, trust-center source/state validation for `Update Device` and `Switch Key`, and small-slot bounded APS retransmission plus duplicate suppression on the clean coordinator/joinable demos now exist in-tree, but the behavior is still a demo trust-center lifecycle rather than full Zigbee 3.0 commissioning.

Deliverables:

- Keep install-code-derived link keys as first-class preconfigured-key mode, with optional ZigBeeAlliance09 fallback only when policy allows it.
- Persist trust-center IEEE, preconfigured-key provenance, inbound APS security counters, and key sequence state.
- Require encrypted Transport Key delivery by default on the joinable HA examples.
- Extend secure rejoin eligibility checks so they depend on retained key state and link-key policy, not just prior short-address knowledge.
- Add network-key update scaffolding: key switch / alternate-key persistence / frame-counter reset rules for future rollout support.
- Tighten coordinator behavior so trust-center commands are delivered only through the correct secured path for the current state.
- Tighten end-device behavior so trust-center commands are accepted only from the expected source and only in the correct commissioning state.

Verification:

- Self-test vectors for install-code CRC and install-code-derived link-key generation.
- Compile the coordinator demo, joinable HA examples, and codec self-test.
  Use `scripts/zigbee_coordinator_compile_matrix.sh` and `scripts/zigbee_joinable_compile_matrix.sh`.
- Manual two-board run: install-code join, retained-key rejoin, replay rejection, and secure trust-center command acceptance.

### Wave 3: Third-Party Coordinator Interoperability Prep

Objective: remove in-tree coordinator assumptions from the device examples and define the adapter boundary for real external-coordinator work.

Status: partially landed. The joinable HA examples already learn or pin trust-center identity by policy macro instead of hard-coding the in-tree coordinator, the coordinator demo now also exposes its trust-center identity plus known-node install-code table through build-time macros instead of fixed sketch edits, serial logs now surface key-state transitions plus commissioning failure state, the coordinator demo can exercise Identify/Identify Query/Trigger Effect plus identify-time `Write Attributes`, `Write Attributes Undivided`, and extended-attribute-discovery flows against discovered HA nodes, the shared HA runtime now carries effect-specific identify state so the light/sensor examples can render distinct blink/breathe/channel-change patterns locally, shared `Write Attributes`, `Write Attributes Undivided`, and `Write Attributes No Response` handling now updates writable HA state such as `IdentifyTime` while returning explicit read-only status for known read-only attributes and keeping undivided writes atomic, extended attribute discovery now reports the actual read/write/report capabilities of the modeled HA attributes in-tree, the coordinator demo now walks a ZHA-like early interview through Node Descriptor, Power Descriptor, Active EP, Simple Descriptor, and richer Basic-cluster reads instead of skipping straight to endpoint discovery, and it now verifies reporting setup by reading the reporting configuration back after configure-reporting succeeds, configurable scan masks and commissioning timing windows now exist for external-coordinator bring-up, the expected coordinator-facing packet flow is documented in `docs/ZIGBEE_EXTERNAL_COORDINATOR_FLOW.md`, the first-attempt ZHA recipe is documented in `docs/ZIGBEE_HOME_ASSISTANT_BRINGUP.md`, `scripts/zigbee_joinable_compile_matrix.sh` now verifies the three joinable HA examples under default, pinned strict-external, and learned-trust-center install-code-only policy macros, and `scripts/zigbee_coordinator_compile_matrix.sh` now verifies the coordinator demo under default and stricter install-code-only policy overrides. Real external-coordinator validation is still missing.

Deliverables:

- Keep trust-center identity learned or pinned by configuration, instead of hard-coding the in-tree coordinator IEEE into endpoint logic.
- Move demo-only network assumptions behind explicit compile-time policy macros.
- Normalize serial/log output around join, Transport Key, Update Device, and secure rejoin decisions so packet captures can be matched against behavior.
- Document the coordinator-facing boundary: required MAC behavior, BDB expectations, trust-center exchanges, and the future split between end-device interoperability and coordinator/NCP work.
- Define the next adapter phase for Zigbee2MQTT separately from end-device parity so the remaining work is not conflated.

Verification:

- Confirm the joinable HA examples compile with stricter policy settings such as pinned external-coordinator mode and learned-trust-center install-code-only mode.
  Use `scripts/zigbee_joinable_compile_matrix.sh`.
- Confirm the coordinator demo compiles with overridden trust-center identity and sparse install-code policy tables without sketch edits.
  Use `scripts/zigbee_coordinator_compile_matrix.sh`.
- Confirm docs and feature matrix name the exact missing behavior for ZHA and Zigbee2MQTT interop.
- Capture at least one packet-level expected-flow document for future external-coordinator bring-up.

## Dependency Order

1. Shared commissioning state machine.
2. Trust-center lifecycle hardening on top of that state machine.
3. Third-party coordinator prep once the device-side state and security rules are centralized.

Coordinator/NCP work for Zigbee2MQTT should stay out of this batch. It is a later phase after real end-device interoperability exists.

## Exit Criteria For This Batch

This batch is complete only when:

- the HA joinable examples use shared commissioning logic,
- install-code and trust-center identity are treated as durable policy inputs instead of sketch-local shortcuts,
- secure rejoin behavior is explicitly gated by retained-key policy,
- the docs clearly separate current demo-network behavior from true Zigbee 3.0 interop requirements.

## Explicit Non-Goals

- Claiming full Zigbee 3.0 parity before BDB, trust-center lifecycle, and external-coordinator interop are actually validated.
- Claiming Zigbee2MQTT coordinator adapter support before there is a real serial/NCP compatibility layer.
