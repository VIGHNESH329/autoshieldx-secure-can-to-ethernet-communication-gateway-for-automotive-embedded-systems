# AutoShieldX: A Secure CAN-to-Ethernet Gateway Framework for Intelligent Vehicle Communication

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen.svg)](https://cmake.org/)
[![Hardware Status](https://img.shields.io/badge/Hardware-Fully%20Implemented%20%26%20Validated-success.svg)](#3-hardware--system-architecture)
[![SocketCAN](https://img.shields.io/badge/Linux-SocketCAN%20(can0)-informational.svg)](https://www.kernel.org/doc/Documentation/networking/can.txt)
[![Tests](https://img.shields.io/badge/Tests-24%2F24%20Passing-brightgreen.svg)](#12-automated-test-suite-execution)
[![Android](https://img.shields.io/badge/Android%20App-Ready%20(APK)-blue.svg)](#10-android-mobile-companion-application)

---

## 1. Project Objective
**AutoShieldX** is an end-to-end, hardware-validated automotive cybersecurity research and defense framework. It establishes a multi-layered, zero-trust security gateway between an untrusted in-vehicle Controller Area Network (CAN) bus and high-speed Ethernet vehicle backbones. 

The framework is implemented and verified on real embedded automotive hardware — utilizing an **Arduino Uno R3** (with optional **Raspberry Pi Pico 2** support) as the vehicle ECU transmitter, **MCP2515 CAN controller transceivers** communicating over a physical 500 kbps differential twisted-pair CAN bus, an **HC-SR04 ultrasonic distance sensor**, and a **Raspberry Pi 4 Model B** executing the C++17 Secure Gateway via Linux SocketCAN (`can0`). Telemetry and threat intelligence are broadcast in real time to both an operations web dashboard and an Android mobile companion application.

---

## 2. Problem Statement
The legacy CAN bus standard (ISO 11898) was developed in 1986 without security mechanisms:
1. **Lack of Authentication**: Nodes accept any frame with a matching ID, enabling arbitrary packet injection and ECU spoofing.
2. **Lack of Integrity**: Payloads can be tampered with in transit without detection.
3. **No Freshness**: Valid packets recorded from the bus can be retransmitted later (replay attack) to force actuators (e.g. unintended acceleration or brake disablement).
4. **Arbitration Priority Vulnerability**: Dominant CAN IDs (`0x000` or `0x100`) always win bus arbitration, allowing trivial Denial-of-Service (DoS) bus flooding.
5. **Direct External Connectivity Risk**: Connecting vulnerable CAN buses to telematics, infotainment, and Ethernet without isolation exposes the physical drivetrain to remote vehicular cyberattacks.

AutoShieldX defends against these threats through a 6-stage defense-in-depth secure gateway that enforces physical sensor parameter validation, cryptographic HMAC-SHA256 authentication (via the AX-CTP multi-frame transport protocol), monotonic replay prevention, and behavioral machine learning anomaly detection before any CAN message is bridged to the vehicle Ethernet network.

---

## 3. Hardware & System Architecture

### 3.1 Physical Hardware Architecture
```
[ Physical Sensors ]
  - HC-SR04 Ultrasonic Distance Sensor (D7 Trig / D6 Echo)
  - Throttle Potentiometer (A0) / Kinematic Telemetry Engine
        │
        ▼
[ Vehicle ECU Node: Arduino Uno R3 / Raspberry Pi Pico 2 ]
  - Real-time Sensor Acquisition & Telemetry Packing
  - Monotonic Sequence Counter (Anti-Replay)
  - Hardware RFC 2104 HMAC-SHA256 Signing Engine
  - AX-CTP Fragmenter (CAN ID 0x100 Telemetry + 0x101 Auth Fragments)
        │ SPI Bus (MOSI:D11, MISO:D12, SCK:D13, CS:D10)
        ▼
[ MCP2515 CAN Controller #1 + TJA1050 Transceiver ]
        │
        ▼ CAN_H / CAN_L (500 kbps, 120Ω Terminated Twisted Pair)
[ MCP2515 CAN Controller #2 + TJA1050 Transceiver ]
        │ SPI0 Bus (MISO:GPIO9, CS:GPIO8, SCK:GPIO11, MOSI:GPIO10, INT:GPIO25)
        ▼
[ Secure Gateway Node: Raspberry Pi 4 Model B ]
  - Linux SocketCAN (can0) Interface Driver
  - AX-CTP Multi-Frame Reassembler & Fragment Validator
  - 6-Stage Defense-in-Depth Security Pipeline:
      ├─ Stage 1: Frame Syntactic & Physical Semantic Bounds Validation
      ├─ Stage 2: HMAC-SHA256 Cryptographic Authentication (Zero-Trust)
      ├─ Stage 3: Monotonic Sequence Replay Protection
      ├─ Stage 4: ML Behavioral Anomaly Detection (<5 µs Embedded C++)
      ├─ Stage 5: Decision Arbitration Engine
      └─ Stage 6: Ethernet Bridge & Forensics Logger
        │
        ├────────────────────────┬────────────────────────┐
        ▼ Ethernet / WiFi        ▼ WiFi UDP (Port 16000)  ▼ Local JSON Log
[ Operations Web Dashboard ] [ Android Companion App ] [ AWS Mock Telemetry ]
  (HTTP 8080 / Dark Mode)     (output/app-debug.apk)    (Offline Cloud Batch)
```

### 3.2 Dual-Mode Operational Design
AutoShieldX is engineered with complete dual-mode operational support:
1. **Full Physical Hardware Mode**: Physical Arduino Uno R3 / Pico 2 transmitting live sensor data across physical MCP2515 modules to a Raspberry Pi 4 running SocketCAN (`can0`).
2. **Hardware-Free Standalone / Simulation Mode**: For presentation environments, rapid testing, or continuous integration without physical wiring, AutoShieldX provides:
   - A standalone web demo in `demo_standalone/` running realistic kinematics and deterministic anomaly generators with zero external dependencies.
   - A C++ software loopback mode using UDP sockets (`SimulatedCanChannel`) to run the complete C++ gateway and ECU simulator on any laptop or development workstation.

---

## 4. Hardware & Software Components

| Component | Target Hardware / Path | Technical Role & Implementation |
|---|---|---|
| **Vehicle ECU Firmware** | Arduino Uno R3 (`firmware/arduino/arduino_ecu.ino`) | Embedded C++ firmware with hardware SPI, MCP2515 driver, HC-SR04 ultrasonic sensor driver, RFC 2104 HMAC-SHA256 calculation, and AX-CTP fragmentation. |
| **Alternative ECU Firmware** | Raspberry Pi Pico 2 (`firmware/pico2/`) | Bare-metal RP2350 C++ firmware using Pico SDK with hardware SPI and dual-core telemetry generation. |
| **Physical Sensors** | HC-SR04, Potentiometer, SW-420 | Real-time obstacle distance measurement (2 cm – 400 cm), physical throttle input, and vibration sensing. |
| **CAN Transceivers** | Dual MCP2515 + TJA1050 Modules | SPI-to-CAN conversion at 500 kbps over 120-Ohm terminated differential twisted pair. |
| **Secure Gateway** | Raspberry Pi 4 (`src/gateway/`, `build_pi/secure_gateway`) | High-performance C++17 gateway executing the 6-stage security pipeline on Linux SocketCAN (`can0`). |
| **CAN Transport (AX-CTP)**| `include/autoshieldx/can/can_transport.hpp`<br>`src/can/can_transport.cpp` | AutoShieldX CAN Transport Protocol reassembling fragmented 32-byte HMAC signatures across standard 8-byte CAN frames. |
| **Cryptographic HMAC** | `include/autoshieldx/security/crypto_hmac.hpp`<br>`src/security/crypto_hmac.cpp` | OpenSSL EVP / RFC 2104 SHA-256 HMAC engine with constant-time verification. |
| **Replay Protection** | `include/autoshieldx/security/replay_protection.hpp`<br>`src/security/replay_protection.cpp` | Monotonic counter tracking per CAN ID to block retransmitted old packets. |
| **Frame Validation** | `include/autoshieldx/validation/validation_engine.hpp`<br>`src/validation/validation_engine.cpp` | CAN ID whitelist, DLC=8 check, ultrasonic distance limits, sensor range boundaries, and vehicle speed consistency. |
| **ML Anomaly Detection** | `include/autoshieldx/ml/ml_detector.hpp`<br>`src/ml/ml_detector.cpp`<br>`ml/` | Embedded C++ inference evaluator (<5 µs) + Python scikit-learn training pipeline. |
| **Ethernet Forwarder** | `include/autoshieldx/networking/ethernet_forwarder.hpp`<br>`src/networking/ethernet_forwarder.cpp` | Bridges verified vehicle telemetry to the operations network over UDP. |
| **Operations Dashboard** | `dashboard/` | Real-time dark-mode web monitoring operations center (HTML5, Vanilla CSS, JS, Python HTTP/UDP server). |
| **Android Companion App**| `android/` (`output/app-debug.apk`) | Native Kotlin Android mobile application with HUD telemetry dials, threat counters, and dual UDP/HTTP sync. |
| **Pi Deployment Package**| `autoshieldx_pi_deployment/` & `.zip` | Clean Raspberry Pi 4 cross-deployment archive with build scripts and step-by-step guides. |
| **Standalone Demo** | `demo_standalone/` | Instant hardware-free demonstration suite with deterministic anomaly injector. |

---

## 5. Security Mechanisms

### 5.1 HMAC-SHA256 Cryptographic Authentication & Integrity
- **Algorithm**: Keyed-Hash Message Authentication Code using SHA-256 (RFC 2104).
- **Canonical Input Format**: `[CAN_ID (4B BE)] | [Sequence (4B BE)] | [DLC (1B)] | [Payload (8B)]`.
- **AX-CTP Multi-Frame Transport**:
  - **Frame 0 (CAN ID `0x100`)**: Telemetry payload (8 bytes: Speed, RPM, Throttle, Temp, Sequence).
  - **Frames 1..6 (CAN ID `0x101`)**: 6 fragmented container frames transporting the 32-byte (256-bit) HMAC-SHA256 signature across standard 8-byte CAN frames.
- **Security Properties**:
  - **Authentication**: Verifies the sender holds the Pre-Shared Secret Key (PSK). Rogue injection nodes cannot forge valid MAC tags.
  - **Integrity**: Guarantees that the CAN ID, sequence number, and payload have not suffered 1-bit tampering.
  - **Side-Channel Protection**: Verified using constant-time memory comparisons to prevent timing side-channel attacks.

### 5.2 Monotonic Sequence Replay Protection
- An adversary capturing a valid signed frame on the bus cannot be blocked by HMAC alone because the replayed frame already has a valid signature.
- AutoShieldX maintains an internal monotonic sequence counter $S_{last}$ for each CAN ID.
- **Rule**: If incoming sequence $S_{in} \le S_{last}$, the packet is immediately **REJECTED** as a replay attack.

### 5.3 Frame Syntactic & Physical Semantic Validation
Configured via `config/gateway_config.json`:
- **Allowed CAN IDs**: Whitelist (`0x100`, `0x101`, `0x200`, `0x300`, `0x400`).
- **Payload Length**: Exactly 8 bytes (DLC = 8).
- **Physical Sensor Limits**:
  - Speed: $0.0 \le v \le 250.0$ km/h
  - RPM: $0 \le \text{RPM} \le 8,000$
  - Temperature: $-40.0 \le T \le 125.0$ °C
  - Ultrasonic Distance: $2.0 \le d \le 400.0$ cm
  - Vibration: $0.0 \le a \le 5.0$ G
  - Steering: $-540.0^\circ \le \theta \le +540.0^\circ$
  - Wheel Speed: $0.0 \le v_w \le 260.0$ km/h
- **Physical Consistency Check**: $|v_{\text{speed}} - v_{\text{wheel}}| \le 35.0$ km/h (detects wheel slip or sensor manipulation).

---

## 6. Machine Learning Anomaly Detection Component

### 6.1 Architectural Principle
> **CRITICAL RULE**: Machine Learning is a secondary behavioral defense layer. It is **NEVER** used as a replacement for cryptographic authentication.

### 6.2 Feature Vector
The ML module extracts 10 features per incoming CAN frame:
1. `message_frequency_hz`: Calculated over a 1.0-second sliding arrival window.
2. `can_id`: Decimal identifier.
3. `speed_kmh`: Vehicle speed.
4. `rpm`: Engine revolutions per minute.
5. `temperature_c`: Coolant temperature.
6. `vibration_g`: Acceleration in G (vibration / shock).
7. `steering_deg`: Steering wheel angle.
8. `wheel_speed_kmh`: Wheel encoder speed.
9. `sequence_delta`: Gap between current and previous sequence counter.
10. `time_delta_ms`: Inter-frame arrival delta in milliseconds.

### 6.3 Pipeline Workflow
1. **Dataset Generation** (`ml/dataset/generate_dataset.py`): Generates 4,000 normal driving samples and 1,750 labeled attack samples across all 5 threat categories.
2. **Model Training** (`ml/training/train_model.py`): Trains an Isolation Forest / Multi-Variate Boundary Classifier. Evaluates metrics and exports model weights to `ml/inference/model_weights.json`.
3. **Inference**:
   - Python: `ml/inference/ml_detector.py` provides standalone inference.
   - C++: `EmbeddedMlDetector` in `src/ml/ml_detector.cpp` loads the exported JSON thresholds and evaluates incoming frames in $< 5 \ \mu\text{s}$ per frame on the Raspberry Pi 4.

---

## 7. Five Controlled Laboratory Attack Simulators

| Attack Simulator | Source File | Attack Vector Description | Gateway Detection Stage |
|---|---|---|---|
| **Attack 1: Injection & Spoofing** | `attacks/injection/attack_injection.cpp` | Injects rogue CAN IDs (e.g. `0x666`), frames with forged HMACs, and frames with missing HMACs. | **Stage 1** (ID Whitelist) & **Stage 2** (`REJECT_HMAC_AUTHENTICATION_FAILED`) |
| **Attack 2: Replay Attack** | `attacks/replay/attack_replay.cpp` | Captures authentic signed frame (Seq 10), lets stream advance to Seq 15, and replays stale frame. | **Stage 3** (`REJECT_REPLAY_DETECTED` where $10 \le 15$) |
| **Attack 3: DoS / Flooding** | `attacks/dos/attack_dos.cpp` | Bursts 150+ frames at ~300 Hz on dominant CAN ID `0x100` to starve arbitration. | **Stage 4** (`REJECT_ML_ANOMALY_DETECTED` - `DOS_FLOOD_ANOMALY` where rate > 80 Hz) |
| **Attack 4: Fuzzy / Random Traffic** | `attacks/fuzzy/attack_fuzzy.cpp` | Generates pseudorandom CAN IDs, corrupt payload bytes, and arbitrary fake HMACs. | **Stage 1** (`REJECT_FRAME_VALIDATION_FAILED`) & **Stage 2** (HMAC mismatch) |
| **Attack 5: False-Data Manipulation** | `attacks/false_data/attack_false_data.cpp` | Injects impossible sensor signals (Speed 320 km/h, RPM 11,500, Temp 195°C, Speed vs Wheel discrepancy) with authentic HMAC. | **Stage 1** (`REJECT_FRAME_VALIDATION_FAILED` - Physical boundary violations) |

---

## 8. Physical Hardware Wiring & Pinouts

### 8.1 Arduino Uno R3 to MCP2515 CAN Transceiver #1
| MCP2515 Pin | Arduino Uno R3 Pin | Function / Wire Color |
|---|---|---|
| **VCC** | **5V** | 5V DC Power (TJA1050 transceiver requires 5V) |
| **GND** | **GND** | Common Ground |
| **CS** | **Pin 10** | SPI Chip Select |
| **SO (MISO)** | **Pin 12** | SPI MISO |
| **SI (MOSI)** | **Pin 11** | SPI MOSI |
| **SCK** | **Pin 13** | SPI Clock |
| **INT** | **Pin 2** | Interrupt (Optional) |

### 8.2 HC-SR04 Ultrasonic Distance Sensor to Arduino Uno R3
| HC-SR04 Pin | Arduino Uno R3 Pin | Function |
|---|---|---|
| **VCC** | **5V** | 5V DC Power |
| **GND** | **GND** | Common Ground |
| **TRIG** | **Pin 7** | Ultrasonic Pulse Trigger |
| **ECHO** | **Pin 6** | Echo Pulse Duration (Pulse Width) |

### 8.3 Raspberry Pi 4 Model B to MCP2515 CAN Transceiver #2
| MCP2515 Pin | Raspberry Pi 4 Physical Pin | GPIO / Function |
|---|---|---|
| **VCC** | Pin 2 or Pin 4 | 5V DC Power |
| **GND** | Pin 6, 9, 14, 20, or 25 | Common Ground |
| **CS** | Pin 24 | GPIO 8 (SPI0 CE0) |
| **SO (MISO)** | Pin 21 | GPIO 9 (SPI0 MISO) |
| **SI (MOSI)** | Pin 19 | GPIO 10 (SPI0 MOSI) |
| **SCK** | Pin 23 | GPIO 11 (SPI0 SCLK) |
| **INT** | Pin 22 | GPIO 25 (Hardware Interrupt) |

### 8.4 CAN Bus Interconnect (ISO 11898-2)
- Connect **CAN_H** of Arduino MCP2515 to **CAN_H** of Raspberry Pi MCP2515.
- Connect **CAN_L** of Arduino MCP2515 to **CAN_L** of Raspberry Pi MCP2515.
- Ensure the **120Ω termination jumpers** (J1) are closed on **both** MCP2515 modules.

---

## 9. Live Hardware Execution Guide

### Step 1: Flash Arduino Uno R3 Firmware
1. Open `firmware/arduino/arduino_ecu.ino` in Arduino IDE.
2. Verify MCP2515 crystal frequency:
   - For **8 MHz** crystal (marked `8.000`): Keep `#define MCP_CRYSTAL_8MHZ 1`.
   - For **16 MHz** crystal (marked `16.000`): Set `#define MCP_CRYSTAL_8MHZ 0`.
3. Select board **Arduino Uno**, choose the COM port, and click **Upload**.
4. Open the Serial Monitor at **115200 baud** to verify transmission:
   ```
   [MCP2515] Initialized successfully in NORMAL mode at 500 kbps (8 MHz crystal).
   [Arduino 1 TX] Seq: 1 | Spd: 25.4 km/h | RPM: 1820 | Temp: 78.5 C | Dist: 18.4 cm | Mode: AUTH (0x100+0x101)
   ```

### Step 2: Configure SocketCAN on Raspberry Pi 4
In `/boot/firmware/config.txt` (or `/boot/config.txt`), add:
```ini
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25
```
Reboot the Pi, then initialize the `can0` interface:
```bash
sudo ip link set can0 up type can bitrate 500000
candump can0
```
Verify incoming frames: ID `100` (telemetry) and ID `101` (HMAC fragments) will stream in real time.

### Step 3: Run the AutoShieldX Gateway on Raspberry Pi 4
Deploy the pre-packaged archive `autoshieldx_pi_deployment.zip` to the Pi:
```bash
unzip autoshieldx_pi_deployment.zip -d autoshieldx/
cd autoshieldx
mkdir build && cd build
cmake ..
make -j4
./build/bin/secure_gateway --can can0
```

### Step 4: Run the Operations Dashboard
On your workstation or Raspberry Pi:
```bash
python dashboard/dashboard_server.py 8080 16000
```
Open your browser at `http://localhost:8080` (or `http://<pi-ip>:8080`) to view live speed, RPM, ultrasonic distance, HMAC validation status, threat counters, and ML anomaly telemetry.

---

## 10. Android Mobile Companion Application

AutoShieldX includes a companion native Android application for in-vehicle cyber telemetry and threat monitoring:
- **Pre-Built Debug APK**: Available directly at [`output/app-debug.apk`](output/app-debug.apk).
- **Features**: Real-time vehicle speed and RPM gauges, ultrasonic obstacle warnings, attack detection KPI cards, and event audit feed.
- **Dual Network Synchronization**:
  - **UDP Listener (Port 16000)**: Ingests raw telemetry frames forwarded by the gateway over local Wi-Fi.
  - **HTTP Polling (Port 8080)**: Automatically polls the dashboard REST API (`http://10.0.2.2:8080/api/status` for emulators or physical Pi IP) for synchronized metrics.
- **Building from Source**:
  ```bash
  cd android
  ./gradlew assembleDebug
  ```

---

## 11. Standalone Hardware-Free Demo Mode

If physical hardware is not connected, the complete AutoShieldX presentation can be executed in standalone mode:
```bash
# On Windows:
demo_standalone\run_demo.bat

# On Linux / macOS:
./demo_standalone/run_demo.sh
```
This launches a self-contained web server at `http://localhost:8081` with:
- Realistic 20 Hz vehicle kinematic dynamics.
- An automated, deterministic ML anomaly generator that produces a vibration spike ($3.42\text{ G}$) every 12 seconds to demonstrate live threat detection.
- Zero dependencies outside standard Python 3.

---

## 12. Automated Test Suite Execution

Run the comprehensive 24-test verification suite:
```bash
# On Linux/macOS/Pi:
./build/bin/autoshieldx_tests

# On Windows (MSVC):
.\build\Release\autoshieldx_tests.exe
```

### Complete Test Verification Summary:
```
============================================================
      AUTOSHIELDX VERIFICATION & AUTOMATED TEST SUITE        
============================================================

TEST 1:  testHmacGeneration                               >>> PASS
TEST 2:  testHmacVerification                             >>> PASS
TEST 3:  testModifiedMessageDetection                     >>> PASS
TEST 4:  testSequenceValidation                           >>> PASS
TEST 5:  testReplayDetection                              >>> PASS
TEST 6:  testCanFrameValidation                           >>> PASS
TEST 7:  testNormalVehicleTraffic                         >>> PASS
TEST 8:  testInjectionAttack                              >>> PASS
TEST 9:  testReplayAttack                                 >>> PASS
TEST 10: testDosTraffic                                   >>> PASS
TEST 11: testFuzzyTraffic                                 >>> PASS
TEST 12: testFalseDataTraffic                             >>> PASS
TEST 13: testGatewayDecisionLogic                         >>> PASS
TEST 14: testCanTransportFragmentationAndReassembly       >>> PASS
TEST 15: testCanTransportReassembledPipelineAcceptance    >>> PASS
TEST 16: testCanTransportTamperDetection                  >>> PASS
TEST 17: testHmacKeyParsing                               >>> PASS
TEST 18: testPicoHardwareCompatibilityVector              >>> PASS
TEST 19: testAxCtpMissingFragmentRejection                >>> PASS
TEST 20: testAxCtpOutOfOrderFragments                     >>> PASS
TEST 21: testValidationConfigLoading                      >>> PASS
TEST 22: testEthernetForwardingSerialization              >>> PASS
TEST 23: testArduinoHardwareCompatibilityVector           >>> PASS
TEST 24: testUnauthenticatedTelemetryDetection            >>> PASS

============================================================
  TEST RESULTS SUMMARY
  Total Tests Executed : 24
  Total Tests Passed   : 24
  Total Tests Failed   : 0
============================================================
```

---

## 13. License & Authorship
Developed as an automotive cybersecurity research project for intelligent connected vehicles. See the [docs/](docs/) directory for detailed hardware schematics, protocol specifications, and deployment guides.
