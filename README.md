# nRF54L15 Arduino Core

Open-source Arduino board package for **nRF54L15** — the latest-gen Cortex-M33 + RISC-V SoC from Nordic Semiconductor. No Zephyr, no nRF Connect SDK. Just pure register-level C++ on bare metal.

---

## Quick Start

1. Add Boards Manager URL in Arduino IDE:  
   **`https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json`**

2. Install **nRF54L15 Boards** from Tools → Board → Boards Manager

3. Select **XIAO nRF54L15 / Sense** and upload

```cpp
#include <nrf54_all.h>
void setup() { Serial.begin(115200); Serial.println("Hello nRF54!"); }
void loop() {}
```

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
| **Wireless** | BLE advertising, scanning, connections | ✅ |
| | BLE 2M/coded PHY | ✅ |
| | BLE SMP legacy pairing (phone fallback) | ✅ |
| | BLE Channel Sounding (phase ranging) | ✅ |
| | BLE Channel Sounding (2-board, 57-62cm) | ✅ |
| | Thread: leader, child, router | ✅ |
| | Thread: UDP communication | ✅ |
| | Thread: PSK Joiner/Commissioner | ✅ |
| | Thread: CSL sleepy end device | ✅ |
| | Zigbee: coordinator, router, end-device | ✅ |
| | Zigbee: 2-board join | ✅ |
| **Matter** | On/off light over Thread | ✅ |
| | Encrypted IM (AES-CTR) | ✅ |
| | PASE SPAKE2+ commissioning | ✅ |
| | CASE Sigma protocol + fragmentation | ✅ |
| | Software secp256r1 ECC | ✅ |
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

## ⚠️ Known Limitations

| Limitation | Detail |
|---|---|
| **Thread attach** | Common two-board demo partition race is mitigated with child-first attach + deterministic leader fallback; still experimental and needs longer soak/reference-network validation |
| **Thread UDP fragmentation** | Two-board staged UDP smoke validates checked single-frame payloads through 63 bytes; larger fragmented UDP payloads are still experimental |
| **Standard Thread commissioning** | MeshCoP Joiner/Commissioner examples now compile and report support status, but the staged core still ships those roles disabled until DTLS/secure transport is enabled and tested |
| **BLE LE Secure Connections** | Faster software secp256r1 does not enable LESC by itself. The controller still only implements legacy SMP confirm/random/key-distribution, so SC-only centrals still need the missing LESC Public Key / DHKey Check / f4-f5-f6-g2 flow |
| **CRACEN PK engine** | Hardware ECDSA / P-256 acceleration still needs proprietary Nordic microcode |
| **ECDSA speed** | Software ECC now uses Barrett reduction and measures about 0.84 s sign / 1.76 s verify on board — workable for demos, still much slower than dedicated hardware |
| **OpenThread radio** | Radio/diag examples now compile with the staged core; PAL is still experimental and needs longer two-board soak before production use |

---

## 📦 Installation

### Arduino IDE
1. **File → Preferences → Additional Boards Manager URLs**  
   Add: `https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json`
2. **Tools → Board → Boards Manager** → search "nRF54L15" → Install
3. **Tools → Board** → select your board

### Arduino CLI
```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/lolren/nrf54-arduino-core/main/package_nrf54l15clean_index.json

arduino-cli core update-index
arduino-cli core install nrf54l15clean:nrf54l15clean

# Compile and upload
arduino-cli compile --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 MySketch
arduino-cli upload -p /dev/ttyACM0 --fqbn nrf54l15clean:nrf54l15clean:xiao_nrf54l15 MySketch
```

### Linux udev (for upload)
```bash
# From the installed package:
~/.arduino15/packages/nrf54l15clean/hardware/nrf54l15clean/*/tools/setup/install_linux_host_deps.sh --udev
```

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

## 📊 Performance

| Operation | Time | Method |
|---|---|---|
| Field multiply (100x) | 12 ms | 256-bit schoolbook multiply + Barrett reduction |
| Public-key derive / keygen | 785 ms | Software secp256r1 scalar multiply |
| ECDSA sign | 839 ms | Software secp256r1 |
| ECDSA verify | 1764 ms | Software secp256r1, two scalar multiplies |
| ECDH shared secret | 903 ms | Software secp256r1 |
| CRACEN IKG keygen | 0 ms | Hardware DRBG |

This speedup makes software P-256 practical for staged Matter flows and other on-device crypto work. It does not, by itself, turn on BLE LE Secure Connections: the SMP LESC message flow is still not implemented in the controller.

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
