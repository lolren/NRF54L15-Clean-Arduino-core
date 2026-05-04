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
| | BLE SMP pairing (phone) | ✅ |
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
| | ECDSA sign (21 s) / verify (50 s) | ✅ |
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
| **BLE 2-board pairing** | SMP connection works; full JustWorks handshake needs L2CAP routing fix |
| **Thread partitions** | Two boards may form separate networks; 8 s upload delay helps |
| **CRACEN PK engine** | ECDSA hardware acceleration needs proprietary Nordic microcode |
| **NIST fast reduction** | bnMul at 3 ms (bit-level long division); sub-word carry blocks optimization |
| **ECDSA speed** | Software ECC at 21 s sign / 50 s verify — acceptable for demos, not production |
| **OpenThread radio** | 7 radio diag examples need full OT radio stack (linker errors) |

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
| bnMul (mod p) | 3 ms | Bit-level long division |
| Point double (Jacobian) | ~42 ms | 14 bnMuls |
| Point add (mixed Jacobian) | ~42 ms | 14 bnMuls |
| Scalar multiply (windowed) | ~21 s | 256 doubles + ~16 adds |
| ECDSA sign | ~21 s | 1 scalar mul |
| ECDSA verify | ~50 s | 2 scalar muls |
| CRACEN IKG keygen | 0 ms | Hardware DRBG |

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
