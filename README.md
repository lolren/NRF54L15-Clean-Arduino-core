# nRF54L15 Arduino Core

Open-source Arduino board package for **nRF54L15** — the latest-gen Cortex-M33 + RISC-V SoC from Nordic Semiconductor. No Zephyr, no nRF Connect SDK. Just pure register-level C++ on bare metal.

---

## Quick Start

1. Add Boards Manager URL in Arduino IDE:  
   ```
   https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json
   ```

2. Install **nRF54L15 Boards** from Tools → Board → Boards Manager

3. Select **XIAO nRF54L15 / Sense** and upload

```cpp
#include <nrf54_all.h>
void setup() { Serial.begin(115200); Serial.println("Hello nRF54!"); }
void loop() {}
```

---

## What to Expect From This Core

This is a **bare-metal, register-level Arduino core** — no Zephyr, no nRF Connect SDK, no vendor HAL. Every peripheral driver is written from scratch against the nRF54L15 datasheet. That means:

**Strengths:**
- Full control over the hardware — no opaque vendor layers
- Small binary footprint (no RTOS overhead unless you opt into Thread/Matter)
- BLE is production-quality for advertising, scanning, connections, GATT, and the current LE Secure Connections Just Works path. Channel sounding works on two-board in Mode 2/PBR only. See [BLE status](#ble-status)
- All standard Arduino peripherals verified: GPIO, PWM, ADC, I2C, SPI, UART, I2S, PDM
- VPR RISC-V coprocessor fully usable via the SoftPeripheral SDK
- Crypto: AES-CCM, AES-ECB, PBKDF2 all hardware-accelerated through CRACEN

**Limitations you should know about:**
- ECC (secp256r1) is **software-only** — the CRACEN PK engine needs proprietary Nordic microcode that isn't publicly available. Thread/Matter pairing takes 2–5 seconds of CPU-bound crypto. See [Why Software ECC](#-why-software-ecc-and-what-it-means-for-pairing)
- Thread and Matter are **experimental compile targets**, not functional protocol stacks. See [Thread](#thread-experimental) and [Matter](#matter-experimental) status sections
- Zigbee is **functional but incomplete** — coordinator/router/end-device roles work, but many ZCL clusters are missing. See [Zigbee status](#zigbee-status)
- BLE LE Secure Connections currently covers verified Just Works, fixed-PIN, and two-board numeric-comparison pairing on the clean BLE path, plus broader authenticated-pairing plumbing that still needs wider host interoperability validation. See [BLE status](#ble--production-quality)
- P2 GPIO port lacks interrupt/wake capability (hardware limitation of the nRF54L15)

**Who this core is for:** Developers who want bare-metal access to the nRF54L15, are comfortable reading datasheets, and don't mind that some protocol stacks are still maturing. If you need production Thread/Matter/Zigbee today, use Nordic's nRF Connect SDK instead.

---

## Supported Boards

| Board | Identifier |
|---|---|
| XIAO nRF54L15 / Sense | `xiao_nrf54l15` |
| HOLYIOT-25008 Module | `holyiot_25008_nrf54l15` |
| HOLYIOT-25007 Module | `holyiot_25007_nrf54l15` |
| Generic 36-pad Module | `generic_nrf54l15_module_36pin` |

---

## ✨ Feature Matrix

| Category | Feature | Status |
|---|---|---|
| **Wireless** | BLE: advertising, scanning, connections [¹](#ble-status) | ✅ |
| | BLE: 1M, 2M, Coded PHY [¹](#ble-status) | ✅ |
| | BLE: ATT/GATT + Bluefruit API [¹](#ble-status) | ✅ |
| | BLE: LE Secure Connections Just Works + bonding [¹](#ble-status) | ✅ |
| | BLE: authenticated/OOB pairing + host interop [¹](#ble-status) | ⚠️ |
| | BLE: channel sounding [¹](#ble-status) | ⚠️ |
| | Thread: leader, child, router [²](#thread-experimental) | ⚠️ |
| | Thread: UDP communication [²](#thread-experimental) | ⚠️ |
| | Thread: PSK Joiner/Commissioner [²](#thread-experimental) | ⚠️ |
| | Thread: CSL sleepy end device [²](#thread-experimental) | ⚠️ |
| | Zigbee: coordinator, router, end-device [³](#zigbee-status) | ⚠️ |
| | Zigbee: 2-board join [³](#zigbee-status) | ⚠️ |
| **Matter** | On/off light over Thread [⁴](#matter-experimental) | ⚠️ |
| | Encrypted IM (AES-CTR) [⁴](#matter-experimental) | ⚠️ |
| | PASE SPAKE2+ commissioning [⁴](#matter-experimental) | ⚠️ |
| | CASE Sigma protocol + fragmentation [⁴](#matter-experimental) | ⚠️ |
| | Software secp256r1 ECC [⁴](#matter-experimental) | ⚠️ |
| **Crypto** | CRACEN hardware RNG | ✅ |
| | CRACEN IKG key generation (0 ms) | ✅ |
| | ECDSA sign (~0.84 s) / verify (~1.76 s) | ✅ |
| | PBKDF2-HMAC-SHA256 | ✅ |
| | AES-ECB, AES-CCM hardware | ✅ |
| **Peripherals** | GPIO, PWM, ADC, I2C, SPI, UART | ✅ |
| | I2S, PDM (microphone) | ✅ |
| | QDEC (rotary encoder) | ✅ |
| | NFC-A tag | ✅ |
| | Temperature sensor | ✅ |
| | Comparator, LPCOMP | ✅ |
| | Watchdog timer | ✅ |
| **System** | VPR RISC-V coprocessor | ✅ |
| | VPR SoftPeripheral SDK + sQSPI | ✅ |
| | Deep sleep / System OFF | ✅ |
| | DPPI hardware event system | ✅ |
| | Tamper detection | ✅ |
| | KMU key management | ✅ |

---

<a id="ble-status"></a>
## BLE — Production Quality

BLE is the most mature wireless stack in this core. The entire controller is written from scratch against the nRF54L15 datasheet — Link Layer state machine, radio scheduling, ATT/GATT, L2CAP, SMP security, and the Bluefruit-compatible Arduino API are all implemented at register level.

The nRF54L15 radio supports LE 1M, LE 2M, LE Coded PHYs, and Channel Sounding.

Full BLE-compliance work is paused while the remaining power and interoperability regressions are closed. The resume checklist is kept in [`docs/BLE_COMPLIANCE_RESUME.md`](docs/BLE_COMPLIANCE_RESUME.md) so future BLE work starts from the known-good test matrix instead of rediscovering old regressions.

### What's fully implemented

- [x] Advertising — connectable, scannable, non-connectable, and directed
- [x] Scanning — active and passive, with whitelist filtering
- [x] Connections — central and peripheral simultaneously, up to 20 concurrent links
- [x] LE 1M, LE 2M, and LE Coded PHY (S2 and S8 coding)
- [x] PHY update procedure (negotiate 1M ↔ 2M ↔ Coded mid-connection)
- [x] Data Length Extension — up to 251-byte payloads
- [x] ATT/GATT server and client — full Bluefruit API (services, characteristics, descriptors)
- [x] L2CAP connection-oriented channels
- [x] LE Ping and connection parameter update
- [x] Simultaneous central and peripheral roles
- [x] DPPI-driven background advertising — radio events scheduled via hardware without CPU intervention
- [x] LE Secure Connections Just Works pairing/bonding — two-board fresh pair, bond persistence, bonded reconnect encryption, encrypted notifications, and encrypted writes

### What's partially implemented

- [ ] **SMP authenticated pairing modes** — LE Secure Connections Just Works is verified on two-board setups. Fixed six-digit PIN pairing and two-board numeric comparison are now validated on the raw HAL security demos, and the Bluefruit passkey / numeric-comparison callback paths compile through `Security > pairing_pin`, `Security > pairing_passkey`, `Central > central_pairing`, and `Central > central_pairing_pin`. Broader phone/host interoperability, passkey-entry validation against non-core peers, OOB, and optional identity/signing key distribution still need more work.
- [ ] **Channel Sounding — Mode 2 (PBR / phase-based ranging)** — works on two-board setups, achieving ~57–62 cm accuracy. Requires the VPR RISC-V coprocessor to handle DFE IQ sample capture and processing at radio speed. The M33 CPU alone can't keep up with the subevent timing. Initiator and reflector roles are implemented, but the procedure setup currently uses a hardcoded internal demo profile rather than the full CS configuration workflow.
- [ ] **Channel Sounding — Mode 1 (RTT)** — radio registers configured, raw RTT subevents fire, but timing calibration and distance extraction haven't been validated. No end-to-end RTT distance measurement demo exists yet.
- [ ] **Channel Sounding — Mode 3 (hybrid RTT+PBR)** — data structures for hybrid steps exist, but the combined mode hasn't been tested.
- [ ] **Channel Sounding security** — CS step encryption (AES-CCM per-step) is not yet implemented. All CS procedures run unencrypted.
- [ ] **OOB pairing** — scaffolded in the SMP layer but never tested with a real OOB data source.

For production applications needing certification-level Bluetooth behavior, Nordic's Zephyr-based **nRF Connect SDK** is still the safer path — it ships with a fully validated Bluetooth LE controller, certified channel sounding support, and qualification listings this bare-metal core does not claim.

---

## Thread & Matter & Zigbee Status

These three protocol stacks are in active development. Thread and Matter are the least mature — they compile and pass local smoke tests but haven't been validated end-to-end against real commissioners. Zigbee is further along with functional coordinator/router/end-device roles and 16 example sketches, but still has gaps. BLE is production-quality and remains the primary focus.

---

<a id="thread-experimental"></a>
### 🧵 Thread — Experimental

The Thread stack is an early-stage port of OpenThread core that compiles, links, and can form/join partitions on two-board setups. It is **not** production-ready.

**What works:**
- [x] Compile and link (FTD, `OPENTHREAD_FTD=1`)
- [x] Import operational datasets from TLV hex
- [x] Form new partition as Leader
- [x] Attach as Child when a parent is reachable
- [x] Router role upgrade
- [x] Public child-only / router-eligible mode control through the Arduino wrapper
- [x] UDP transport — checked single-frame payloads up to 63 bytes
- [x] Basic two-board ping/pong smoke tests
- [x] Radio PAL support for source match, scheduled receive, CSL state, coex metrics, channel TX power limits, and one-peer enhanced-ACK probing configuration

**What doesn't (yet):**
- [ ] End-to-end validation against a real commissioner (Apple Home, Google Home, HA Matter Server)
- [ ] SRP client / service registration (`OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE=0`)
- [ ] mDNS / DNS-SD advertising (`OPENTHREAD_CONFIG_MULTICAST_DNS_ENABLE=0`)
- [ ] DNS client (`OPENTHREAD_CONFIG_DNS_CLIENT_ENABLE=0`)
- [ ] Border Agent / Router roles
- [ ] DTLS / secure transport layer
- [ ] Standard MeshCoP Joiner/Commissioner (disabled at compile time)
- [ ] Fragmented UDP payloads (> 63 bytes) — experimental only
- [ ] Power management (CSL sleepy end device and enhanced-ACK probing state are scaffolded, but secure Enhanced ACK IE insertion and sleepy-device validation are not complete)
- [ ] Production soak testing on reference networks

**Known issues:**
- Devices fall back to Leader when they can't find an existing parent on the imported dataset's channel — this is correct Thread FTD protocol behavior, but means a single device on an empty channel forms its own isolated partition

---

<a id="matter-experimental"></a>
### 🏠 Matter — Early Experimental

The Matter support is a **compile-time and minimal-runtime smoke test**, not a functional Matter device. The CHIP SDK header scaffolding is in place, and the On/Off Light data model initializes correctly, but critical network-layer subsystems are not implemented.

**What works:**
- [x] Compile and link against staged CHIP headers
- [x] On/Off Light device type + data model initialization
- [x] Commissioning window opens (PASE SPAKE2+ seed generated)
- [x] Local readiness check passes (`ready_for_on_network_commissioning=1`)
- [x] Manual pairing code and QR code generation
- [x] Encrypted IM layer (AES-CTR) compiles
- [x] CASE Sigma protocol state machine (partial, compiles)
- [x] Software secp256r1 ECC (Barrett reduction, ~0.84s sign / ~1.76s verify)
- [x] Structured staged DNS-SD/SRP record builder for commissionable
      `_matterc._udp` and blocked operational `_matter._tcp` records
- [x] Staged discovery publish lifecycle with publish/unpublish diagnostics

**What doesn't (yet):**
- [ ] mDNS/DNS-SD commissioning advertisement (`_matterc._udp` record is
      built locally but not published)
- [ ] SRP operational service registration
- [ ] Platform DNSSD bridge
- [ ] Operational discovery responder
- [ ] Full CASE session establishment (scaffolded but not validated)
- [ ] End-to-end commissioning against Apple Home / Google Home / HA Matter Server
- [ ] Any real commissioner interaction — never tested on live hardware against a commissioner
- [ ] Cluster persistence across reboots
- [ ] Multi-endpoint support (single On/Off Light endpoint only)

**Honest assessment:** The build menu option is named "Experimental Compile Target" for a reason. It proves the code links and state machines initialize — nothing more. Commissioning window opens locally and the code can now build the DNS-SD/SRP records it would need to publish, but the device is still invisible on the network because mDNS/SRP service publication isn't compiled in. This is a compile-time smoke test, not a commissionable Matter device.

---

<a id="zigbee-status"></a>
### 📡 Zigbee — Good But Imperfect

Zigbee is the most mature of the three 802.15.4 stacks (Thread, Matter, and Zigbee). It has a from-scratch 802.15.4 MAC, NWK, APS, and ZCL implementation with 16 example sketches covering coordinator, router, and end-device roles across lights, sensors, and interoperability demos.

**What works:**
- [x] Coordinator, Router, and End-Device roles
- [x] 2-board join (end device → coordinator)
- [x] MAC association/disassociation
- [x] Network formation and addressing
- [x] APS data transfer and acknowledgements
- [x] ZCL On/Off cluster (client and server)
- [x] ZCL Level Control cluster
- [x] ZCL Identify cluster
- [x] ZCL Temperature Measurement cluster
- [x] ZCL Color Control cluster (scaffolded)
- [x] Basic security (NWK key, link key, APS encryption)
- [x] Rejoin with persistence
- [x] Binding table (static, 8 entries)
- [x] End-device timeout / keepalive
- [x] 16 example sketches

**What doesn't (yet):**
- [ ] ZCL Groups cluster
- [ ] ZCL Scenes cluster
- [ ] ZCL Reporting (attribute reporting engine)
- [ ] ZCL Alarms
- [ ] OTA firmware upgrade
- [ ] Green Power
- [ ] Touchlink commissioning
- [ ] Inter-PAN communication
- [ ] Trust Center link key update / re-keying
- [ ] Install code-based commissioning
- [ ] Multi-hop routing (tree routing only)
- [ ] Network-wide security key rotation
- [ ] Production soak testing with commercial Zigbee coordinators (Hue, Home Assistant ZHA/Z2M)

**Known issues:**
- Binding table is static — entries added at compile time, no runtime ZDO bind/unbind handling
- No coordinator failover — if the coordinator goes down, the network stalls
- Routing is basic tree routing; AODV/route discovery is not implemented
- Security is pre-configured — no dynamic trust center join flow

---

## ⚠️ Other Known Limitations

| Limitation | Detail |
|---|---|
| **BLE LE Secure Connections** | Just Works, fixed-PIN, and two-board numeric comparison are verified on the clean BLE path. Broader phone/host interoperability, passkey-entry validation against non-core peers, OOB, and optional identity/signing key distribution still need more work. |
| **CRACEN PK engine** | Hardware ECDSA / P-256 acceleration still needs proprietary Nordic microcode |
| **ECDSA speed** | Software ECC now uses Barrett reduction and measures about 0.84 s sign / 1.76 s verify on board — workable for demos, still much slower than dedicated hardware |

---

## BLE bandwidth note

Bluefruit `BANDWIDTH_*` profiles now cap negotiated ATT MTU and Data Length per
role instead of always behaving like `MAX`.

- `NORMAL` now stays at `MTU=23` / `DataLength=27` even if the peer is `MAX`.
- `MAX` to `MAX` still negotiates `MTU=247` / `DataLength=251`.
- `requestDataLengthUpdate()` now follows the active role's bandwidth cap, not
  just the raw silicon maximum.

## BLE Secure Connections note

LE Secure Connections Just Works is implemented and two-board verified for the
clean BLE path. The included `BLE > Security > BlePairPeripheral` and `BLE >
Security > BlePairCentral` examples exercise fresh pairing, bond persistence,
bonded reconnect encryption, encrypted notifications, and encrypted writes.

Fixed-PIN pairing is now also wired through the Bluefruit compatibility layer
with `Security > pairing_pin` on the peripheral side and `Central >
central_pairing_pin` on the central side. Dynamic passkey / numeric-comparison
callbacks compile through the current core as well. On the raw HAL side, the
`BlePairPeripheral` / `BlePairCentral` demos also have compile-time validation
modes for fixed-PIN and numeric-comparison pairing, and the numeric-comparison
mode has been verified on two attached nRF54 boards with matching six-digit
values on both sides before encrypted traffic resumes. Broader host
interoperability and OOB are still tracked as future work.

---

## 📦 Installation

### Arduino IDE
1. **File → Preferences → Additional Boards Manager URLs**  
   Add: `https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json`
2. **Tools → Board → Boards Manager** → search "nRF54L15" → Install
3. **Tools → Board** → select your board

### Arduino CLI — Linux
```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json

arduino-cli core update-index
arduino-cli core install nrf54l15clean:nrf54l15clean

# Compile and upload (port is usually /dev/ttyACM0)
arduino-cli compile --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 MySketch
arduino-cli upload -p /dev/ttyACM0 --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 MySketch
```

### Arduino CLI — Windows (PowerShell)
```powershell
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json

arduino-cli core update-index
arduino-cli core install nrf54l15clean:nrf54l15clean

# Find your COM port first:
arduino-cli board list

# Compile and upload (e.g. COM3)
arduino-cli compile --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 MySketch
arduino-cli upload -p COM3 --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 MySketch
```

> **Troubleshooting:** If `core install` says "Platform not found" right after the index downloaded, you probably have a manual copy of the core in your sketchbook folder (`~/Arduino/hardware/nrf54l15clean` on Linux, `%USERPROFILE%\Documents\Arduino\hardware\nrf54l15clean` on Windows). The sketchbook copy takes precedence over Boards Manager and hides the packaged version. Move or delete that folder and try again.

### Linux: udev rules for upload permissions
```bash
~/.arduino15/packages/nrf54l15clean/hardware/nrf54l15clean/*/tools/setup/install_linux_host_deps.sh --udev
```

### Windows: CMSIS-DAP driver
Windows 10/11 should recognize the XIAO nRF54L15 as a CMSIS-DAP device automatically. If upload fails, install the [Zadig](https://zadig.akeo.ie/) tool and replace the driver for the CMSIS-DAP interface with WinUSB.

---

## 🏗️ Architecture

```
nrf54-arduino-core/
├── hardware/nrf54l15clean/nrf54l15clean/
│   ├── cores/nrf54l15/          # Arduino core (GPIO, Serial, Wire, SPI)
│   ├── variants/                # Board pinouts
│   └── libraries/
│       └── Nrf54L15-Clean-Implementation/
│           ├── src/             # HAL, BLE, Thread, Matter, Zigbee, ECC
│           ├── examples/        # 235+ example sketches
│           └── third_party/     # OpenThread, CHIP headers
├── docs/                        # Documentation
├── package_nrf54l15clean_index.json  # Boards Manager index
└── README.md
```

### Single Include
All features accessible through one header:
```cpp
#include <nrf54_all.h>
// Gives you: Secp256r1, Nrf54ThreadExperimental, BleRadio, 
//            ZigbeeRadio, VprSoftperipheralManager, VprSQspi, ...
```

---

## 🔐 Why Software ECC (And What It Means for Pairing)

The nRF54L15 has a hardware crypto accelerator called **CRACEN** that includes a PK engine for P-256 ECDSA. In theory this would make ECC operations nearly instant. In practice, the PK engine requires **proprietary Nordic microcode firmware** to operate — firmware that isn't publicly available outside Nordic's closed nRF Connect SDK.

So we ship a from-scratch software secp256r1 implementation using Barrett reduction. It's optimized but it's still software running on a 128 MHz Cortex-M33:

| Operation | Time | What It Means |
|---|---|---|
| CRACEN IKG keygen | 0 ms | Hardware DRBG feeds the RNG instantly — this works |
| Field multiply (100×) | 12 ms | Barrett reduction at ~12 µs per multiply |
| ECDSA sign | ~0.84 s | Thread Joiner sign, Matter PASE init |
| ECDH shared secret | ~0.90 s | Thread link key exchange |
| ECDSA verify | ~1.76 s | Thread Commissioner verify, Matter CASE |
| Public-key derive | ~0.79 s | Key generation at boot |

### Real-world impact on Thread and Matter pairing

- **Thread Joiner:** Needs one ECDSA sign + one ECDH. Expect **~2 seconds** of crypto overhead before the device even sends its first join request.
- **Matter PASE:** Needs ECDSA sign + ECDH key agreement. Expect **~3 seconds** of crypto before the commissioning window handshake completes.
- **Overall pairing time:** The CPU will spend 2–5 seconds crunching elliptic curve math before any network exchange begins. This is slow but workable for demos and development. It would be frustrating in a production device.

If Nordic ever releases the CRACEN PK microcode publicly (or if a community reverse-engineering effort succeeds), these numbers drop to near-zero and pairing becomes instant. Until then, software ECC is the only option for a fully open-source, no-NDA core.

Software ECC is also used by BLE LE Secure Connections. The Just Works SC message flow is now plumbed through the clean BLE controller, but authenticated SC modes and broader interoperability still need more validation.

---

## 🔧 Development

```bash
git clone https://github.com/lolren/nrf54-arduino-core.git
cd nrf54-arduino-core

# Compile all examples (takes ~30 min)
find hardware/.../examples -name "*.ino" -exec dirname {} \; | sort -u | \
  while read d; do arduino-cli compile --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 "$d"; done
```

---

## 📝 License

MIT — see source files for details. Third-party components (OpenThread, CHIP headers) retain their original licenses.

---

## Credits

Thank you to [@lyusupov](https://github.com/lyusupov) and [@msfujino](https://github.com/msfujino) for the testing, feedback, and technical discussion that helped shape this core.

A special thank you to [@msfujino](https://github.com/msfujino) for spending so much time testing across real hardware and reporting regressions carefully. This core would not be as polished without msfujino.
