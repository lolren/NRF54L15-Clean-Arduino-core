# Thread And Matter Implementation Plan

Status baseline:

- Audit date: `2026-04-26`
- First supported board target: `XIAO nRF54L15 / Sense`
- First runtime direction: `CPUAPP-first`
- Thread upstream: `OpenThread`
- Matter upstream: `connectedhomeip`
- Radio backend: existing `ZigbeeRadio` / IEEE 802.15.4 path

This is the current public plan for Thread and Matter in this repo. Older
phase-by-phase scratch notes were removed because they were written before the
staged OpenThread and Matter foundation work landed.

## Current Claim Level

| Area | Claim today | Not claimed yet |
|---|---|---|
| Thread | Experimental staged OpenThread path with fixed dataset, leader/child/router paths, PSKc/passphrase helpers, and UDP examples. | Production Thread stack, joiner, commissioner, reference-network attach, reboot recovery, sleepy-device depth. |
| Matter | Foundation-only on-network/on-off-light shape with onboarding helpers, Thread dataset export seam, and staged DNS-SD discovery fields. | Real commissioning, real mDNS/SRP registration, discovery, control from commissioner/Home Assistant, secure sessions, reboot/reconnect recovery. |
| VPR | Available as a future offload seam, not the first Thread/Matter owner. | VPR-owned Thread radio/controller or Matter runtime. |

## Architecture Decisions

- `CPUAPP` owns the first OpenThread core and Matter foundation path.
- `ZigbeeRadio` remains the first IEEE 802.15.4 backend for Thread.
- `VPR` is intentionally out of the first Thread radio path.
- `Preferences` is the first settings/persistence backend.
- `CracenRng` provides entropy.
- Existing CRACEN-backed and software fallback crypto glue is used only where
  the staged upstream paths need it.
- Thread Border Router is out of scope for this repo; use an external border
  router for product networks.
- Matter BLE rendezvous is out of first-pass scope; first Matter direction is
  on-network Thread commissioning.

## Phase Checklist

| Box | Phase | Status | Remaining work |
|---|---|---|---|
| [x] | 0. Ownership freeze | Done | Keep ownership docs synchronized with code constants. |
| [x] | 1. OpenThread platform skeleton | Done | Maintain compile coverage when upstream snapshots change. |
| [x] | 2. Real 802.15.4 radio backend | Done | Keep Zigbee regression coverage because Thread shares the same radio path. |
| [x] | 3. Experimental Thread runtime | Partial / experimental | Fixed dataset, role, and UDP examples exist; production validation remains open. |
| [x] | 4. Arduino Thread wrapper | Partial / experimental | Keep API explicitly experimental until reference-network and reboot tests pass. Joiner and Commissioner APIs declared (stubbed until OT core DTLS support is added). |
| [x] | 5. Matter foundation | Foundation done | On-network on/off-light, encrypted IM over Thread (2-board), PASE SPAKE2+ commissioning verified ~45s, PBKDF2-HMAC-SHA256, software secp256r1 ECC (Jacobian + 4-bit windowed, sign 21s/verify 50s). CRACEN IKG keygen 0ms; PK engine needs proprietary microcode. |
| [x] | 6. Matter commissioning | Protocol verified | PASE fully verified on 2 boards (PBKDF exchange + SPAKE2+ commit 22.5s + shared secret). CASE Sigma protocol with message fragmentation compiles and protocol logic verified. Thread PSK Joiner MAC verification passes. | HDK integration, mDNS/SRP, reboot recovery. |
| [ ] | 7. Hardening | Not done | Soak tests, failure recovery, storage migration, interop matrix, and docs for production limits. |

## Thread Next Ticks

- [x] PSK Joiner/Commissioner implemented (MAC verified with PSKd derivation)
- [ ] Validate attach to a reference Thread network through an external border router
- [ ] Add reboot recovery test for saved dataset/settings
- [ ] Expand sleepy-device behavior beyond the current staged runtime
- [ ] Keep Zigbee examples green while Thread shares the 802.15.4 backend

## Matter Next Ticks

- [x] On/off-light model integrated (verified 2-board encrypted IM over Thread)
- [x] PASE SPAKE2+ commissioning flow implemented and verified on 2 boards
- [x] CASE Sigma protocol implemented with message fragmentation
- [x] Thread PSK Joiner/Commissioner implemented (MAC verified)
- [ ] Enable real mDNS/SRP registration and prove discovery from a commissioner
- [ ] Validate with Home Assistant
- [ ] Prove reboot/reconnect recovery after commissioning

## Evidence Pointers

- Thread examples live under
  `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/Thread`.
- Matter examples live under
  `hardware/nrf54l15clean/nrf54l15clean/libraries/Nrf54L15-Clean-Implementation/examples/Matter`.
- Current status rollup lives in
  `docs/NRF54L15_FEATURE_MATRIX.md`.
- Runtime ownership is documented in
  `docs/THREAD_RUNTIME_OWNERSHIP.md` and
  `docs/MATTER_RUNTIME_OWNERSHIP.md`.

## Do Not Claim Yet

- Production Thread support.
- Thread Border Router support.
- Matter commissioning.
- Matter Home Assistant support.
- Matter BLE rendezvous.
- VPR-owned Thread or Matter runtime.
