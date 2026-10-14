# AutoShieldX: A Secure CAN-to-Ethernet Gateway Framework for Intelligent Vehicle Communication

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen.svg)](https://cmake.org/)
[![Status](https://img.shields.io/badge/Status-Phase%201%20Software%20Simulation-orange.svg)](#current-software-only-status)

---

## 1. Project Objective
**AutoShieldX** is an automotive cybersecurity research and defense framework. It develops a multi-layered, zero-trust security gateway between an untrusted in-vehicle Controller Area Network (CAN) bus and high-speed Ethernet vehicle backbones. The framework defends modern connected vehicles against unauthorized frame injection, replay playback, Denial-of-Service (DoS) arbitration starvation, malformed fuzzing, and sensor signal manipulation.

---

## 2. Problem Statement
The legacy CAN bus standard (ISO 11898) was developed in 1986 without security mechanisms:
1. **Lack of Authentication**: Nodes accept any frame with a matching ID, enabling arbitrary packet injection and ECU spoofing.
2. **Lack of Integrity**: Payloads can be tampered with in transit without detection.
3. **No Freshness**: Valid packets recorded from the bus can be retransmitted later (replay attack) to force actuators (e.g. unintended acceleration or brake disablement).
4. **Arbitration Priority Vulnerability**: Dominant CAN IDs (`0x000` or `0x100`) always win bus arbitration, allowing trivial Denial-of-Service (DoS) bus flooding.
5. **Direct External Connectivity Risk**: Connecting vulnerable CAN buses to telematics, infotainment, and Ethernet without isolation exposes the physical drivetrain to remote vehicular cyberattacks.

AutoShieldX solves this through a defense-in-depth secure gateway that enforces physical parameter validation, cryptographic HMAC-SHA256 authentication, monotonic replay prevention, and behavioral machine learning anomaly detection before any CAN message is bridged to the Ethernet vehicle network.

---

## 3. Project Architecture

### 3.1 Target Architecture (Hardware Vision)
```
Raspberry Pi Pico 2
 (ECU Simulator)
        |
     MCP2515
        |
     CAN Bus (500 kbps Differential Twisted Pair)
        |
     MCP2515
        |
Raspberry Pi 4
(Secure Gateway)
        |
     Ethernet (100/1000 Mbps)
        |
     Dashboard (Operations Monitor)
        |
      AWS (Cloud Storage / Audit Logs)
```

### 3.2 Current Implementation Status: Phase 1 (Software Simulation)
```
[ECU Simulator Process] (C++)
   - SimulatedSensorInput (Kinematic dynamics)
   - Monotonic Sequence Counter
   - HMAC-SHA256 Signer
        │
        ▼ (Simulated CAN via UDP Loopback 127.0.0.1:15000)
[AutoShieldX Secure Gateway] (C++)
   ├─ Stage 1: Frame Validation Engine (Boundaries & Whitelist)
   ├─ Stage 2: HMAC-SHA256 Authenticator (Zero-Trust Keyed Hash)
   ├─ Stage 3: Monotonic Replay Protection (Freshness Counter)
   ├─ Stage 4: ML Behavioral Anomaly Detector (Frequency/Flow)
   ├─ Stage 5: Decision Arbitration Engine
   └─ Stage 6: Ethernet Forwarder & Forensics Logger
        │
        ├─► UDP 127.0.0.1:16000 ──► [Dashboard Server & Web UI]
        └─► Local JSON Buffer   ──► [AWS CloudWatch / S3 Mock File]
```

> **IMPORTANT NOTICE:**
> Currently, the project operates in **Software Simulation Mode Only**.
> Real hardware (Raspberry Pi Pico 2, Raspberry Pi 4, MCP2515 CAN transceivers, and physical sensors) is NOT claimed to be active until Phase 2 physical testing.

---

## 4. Software Components

| Component | Directory / Source | Responsibility |
|---|---|---|
| **CAN Data Model** | `include/autoshieldx/can/can_frame.hpp`<br>`src/can/can_frame.cpp` | Standard `CanRawFrame` (Linux `struct can_frame` binary compatible) & `SecureCanMessage` with telemetry codecs. |
| **CAN Channel Abstraction** | `include/autoshieldx/can/can_channel.hpp`<br>`src/can/can_channel.cpp` | `ICanChannel` interface, `SimulatedCanChannel` (UDP loopback), and `SocketCanChannel` blueprint for Linux MCP2515. |
| **Sensor Abstraction** | `include/autoshieldx/ecu/sensor_interface.hpp`<br>`src/ecu/sensor_interface.cpp` | `ISensorInput` interface, `SimulatedSensorInput` (realistic vehicle kinematics), and blueprint for `RealSensorInput`. |
| **ECU Simulator** | `include/autoshieldx/ecu/ecu_simulator.hpp`<br>`src/ecu/ecu_simulator.cpp`<br>`src/ecu/main_ecu.cpp` | Telemetry generation, 8-byte CAN payload packing, monotonic sequencing, and HMAC-SHA256 generation. |
| **Cryptographic HMAC** | `include/autoshieldx/security/crypto_hmac.hpp`<br>`src/security/crypto_hmac.cpp` | OpenSSL EVP / HMAC-SHA256 engine with portable RFC 2104 deterministic fallback and constant-time verification. |
| **Replay Protection** | `include/autoshieldx/security/replay_protection.hpp`<br>`src/security/replay_protection.cpp` | Monotonic counter tracking per CAN ID to block retransmitted old packets. |
| **Frame Validation** | `include/autoshieldx/validation/validation_engine.hpp`<br>`src/validation/validation_engine.cpp` | CAN ID whitelist, DLC=8 verification, sensor range boundaries, and physical cross-sensor consistency checks. |
| **Secure Gateway** | `include/autoshieldx/gateway/security_pipeline.hpp`<br>`src/gateway/security_pipeline.cpp`<br>`src/gateway/main_gateway.cpp` | Orchestrates the 6-stage defense-in-depth pipeline. |
| **ML Anomaly Detection** | `include/autoshieldx/ml/ml_detector.hpp`<br>`src/ml/ml_detector.cpp`<br>`ml/` | Embedded C++ inference evaluator + Python training pipeline and dataset generator. |
| **Ethernet Forwarder** | `include/autoshieldx/networking/ethernet_forwarder.hpp`<br>`src/networking/ethernet_forwarder.cpp` | Pushes validated vehicle telemetry to the Ethernet monitoring network. |
| **Structured Logging & AWS**| `include/autoshieldx/gateway/gateway_logger.hpp`<br>`include/autoshieldx/networking/aws_logger.hpp` | Structured JSON security logs + mock AWS CloudWatch/S3 batch serializer (works 100% offline). |
| **Dashboard** | `dashboard/` | Real-time monitoring server, responsive dark-mode cyber operations UI. |

---

## 5. Security Mechanisms

### 5.1 HMAC-SHA256 Cryptographic Authentication & Integrity
- **Algorithm**: Keyed-Hash Message Authentication Code using SHA-256 (RFC 2104).
- **Canonical Input Format**: `[CAN_ID (4B BE)] | [Sequence (4B BE)] | [DLC (1B)] | [Payload (8B)]`.
- **Security Properties**:
  - **Authentication**: Proves the sender holds the Pre-Shared Secret Key (PSK). Unauthorized injection nodes cannot forge valid MAC tags.
  - **Integrity**: Guarantees that the CAN ID, sequence number, and payload have not suffered 1-bit tampering.
  - **Confidentiality Notice**: HMAC does **NOT** provide encryption. Telemetry remains plaintext to eliminate decryption latency in safety-critical vehicle control and comply with AUTOSAR SecOC specifications.
- **Side-Channel Protection**: Verified using constant-time memory comparisons to prevent timing side-channel attacks.

### 5.2 Monotonic Sequence Replay Protection
- An adversary capturing a valid signed frame on the bus cannot be blocked by HMAC alone because the replayed frame already has a valid signature.
- AutoShieldX maintains an internal monotonic sequence counter $S_{last}$ for each CAN ID.
- **Rule**: If incoming sequence $S_{in} \le S_{last}$, the packet is immediately **REJECTED** as a replay attack.

### 5.3 Frame Syntactic & Physical Semantic Validation
Configured via `config/gateway_config.json`:
- **Allowed CAN IDs**: Whitelist (`0x100`, `0x200`, `0x300`, `0x400`).
- **Payload Length**: Exactly 8 bytes (DLC = 8).
- **Physical Sensor Limits**:
  - Speed: $0.0 \le v \le 250.0$ km/h
  - RPM: $0 \le \text{RPM} \le 8,000$
  - Temperature: $-40.0 \le T \le 125.0$ °C
  - Steering: $-540.0^\circ \le \theta \le +540.0^\circ$
  - Wheel Speed: $0.0 \le v_w \le 260.0$ km/h
  - Vibration: $0.0 \le a \le 5.0$ G
- **Physical Consistency Check**: $|v_{\text{speed}} - v_{\text{wheel}}| \le 35.0$ km/h (catches impossible vehicle slip / sensor manipulation).

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
6. `vibration_g`: SW-420 sensor acceleration in G.
7. `steering_deg`: Steering wheel angle.
8. `wheel_speed_kmh`: Wheel encoder speed.
9. `sequence_delta`: Gap between current and previous sequence counter.
10. `time_delta_ms`: Inter-frame arrival delta in milliseconds.

### 6.3 Pipeline Workflow
1. **Dataset Generation** (`ml/dataset/generate_dataset.py`): Generates 4,000 normal driving samples and 1,750 labeled attack samples across all 5 threat categories.
2. **Model Training** (`ml/training/train_model.py`): Trains an Isolation Forest / Multi-Variate Boundary Classifier. Evaluates metrics and exports model weights to `ml/inference/model_weights.json`.
3. **Inference**:
   - Python: `ml/inference/ml_detector.py` provides standalone inference.
   - C++: `EmbeddedMlDetector` in `src/ml/ml_detector.cpp` loads the exported JSON thresholds and evaluates incoming frames in $< 5 \ \mu\text{s}$ per frame.

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

## 8. Build & Execution Instructions

### 8.1 Prerequisites
- **C++ Compiler**: GCC (g++ 9+), Clang (10+), or MSVC (Visual Studio 2019+). Supports C++17.
- **CMake**: Version 3.14 or newer.
- **Python**: Python 3.8+ (for dataset generation, ML training, and Dashboard server).
- **OpenSSL**: Optional (if found, links `OpenSSL::Crypto`; otherwise uses built-in RFC 2104 SHA-256/HMAC engine automatically).

### 8.2 Building the Project (CMake)
```bash
# 1. Create and enter build directory
mkdir build
cd build

# 2. Configure with CMake
cmake ..

# 3. Compile all targets
cmake --build . --config Release
```

On Windows with MSVC, binaries are placed in `build/bin/Release/` or `build/bin/`.
On Linux/macOS or MinGW, binaries are placed directly in `build/bin/`.

---

## 9. Running the Complete System (Step-by-Step)

### Step 1: Start the Operations Dashboard Server
In **Terminal 1**:
```bash
python dashboard/dashboard_server.py
```
*Open your web browser at `http://127.0.0.1:8080` to view the live dashboard.*

### Step 2: Start the Secure Gateway
In **Terminal 2**:
```bash
# On Linux/macOS/MinGW:
./build/bin/secure_gateway

# On Windows (MSVC):
.\build\bin\Release\secure_gateway.exe
```
*The gateway initializes the 6-stage defense pipeline and listens on UDP port 15000.*

### Step 3: Start the ECU Simulator
In **Terminal 3**:
```bash
# On Linux/macOS/MinGW:
./build/bin/ecu_simulator

# On Windows (MSVC):
.\build\bin\Release\ecu_simulator.exe
```
*The ECU simulator begins transmitting authenticated telemetry at 20 Hz. Watch the dashboard gauges update live!*

### Step 4: Run the Attack Simulators (Controlled Lab Testing)
In **Terminal 4**, launch any of the attack modules:

#### Run Attack 1 (CAN Injection & Spoofing):
```bash
./build/bin/attack_injection
# Expected: Gateway logs REJECT_HMAC_AUTHENTICATION_FAILED and drops frames.
```

#### Run Attack 2 (Replay Attack):
```bash
./build/bin/attack_replay
# Expected: Gateway logs REJECT_REPLAY_DETECTED for stale sequences.
```

#### Run Attack 3 (CAN DoS Flooding):
```bash
./build/bin/attack_dos
# Expected: Gateway ML engine flags DOS_FLOOD_ANOMALY and drops burst traffic.
```

#### Run Attack 4 (Fuzzy Traffic):
```bash
./build/bin/attack_fuzzy
# Expected: Gateway drops unwhitelisted IDs and malformed frames.
```

#### Run Attack 5 (False-Data Manipulation):
```bash
./build/bin/attack_false_data
# Expected: Gateway rejects physically impossible sensor values.
```

---

## 10. Running ML Training & Inference

```bash
# 1. Generate synthetic automotive dataset
python ml/dataset/generate_dataset.py

# 2. Train anomaly detection model and export weights
python ml/training/train_model.py

# 3. Run sample inference CLI test
python ml/inference/ml_detector.py
```

---

## 11. Automated Test Suite Execution & Expected Output

Run the automated test runner:
```bash
# On Linux/macOS/MinGW:
./build/bin/autoshieldx_tests

# On Windows (MSVC):
.\build\bin\Release\autoshieldx_tests.exe

# Or via CTest:
ctest --output-on-failure
```

### Expected Output for All 13 Tests:
```
============================================================
      AUTOSHIELDX VERIFICATION & AUTOMATED TEST SUITE        
============================================================

TEST 1: testHmacGeneration
Purpose: Verify HMAC-SHA256 generation using shared secret key.
Expected Output: Exact RFC 4231 standard digest match.
  Computed: 5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
  Expected: 5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
>>> RESULT: PASS

TEST 2: testHmacVerification
Purpose: Verify constant-time verification accepts genuine tags and rejects invalid ones.
Expected Output: Identical tags return true; mismatched tags return false.
>>> RESULT: PASS

TEST 3: testModifiedMessageDetection
Purpose: Verify that modifying 1 bit of CAN payload causes HMAC mismatch.
Expected Output: Tampered payload fails verification; Original passes.
>>> RESULT: PASS

TEST 4: testSequenceValidation
Purpose: Verify monotonic sequence validation advances correctly.
Expected Output: Sequences 1, 2, 3 accepted; counter advances.
>>> RESULT: PASS

TEST 5: testReplayDetection
Purpose: Verify replay protection rejects sequence <= last accepted.
Expected Output: Stale sequence 10 and 9 rejected as replay when last is 12.
>>> RESULT: PASS

TEST 6: testCanFrameValidation
Purpose: Test frame validation engine against valid and invalid parameters.
Expected Output: Out-of-range sensor values and unauthorized IDs rejected.
>>> RESULT: PASS

TEST 7: testNormalVehicleTraffic
Purpose: Verify legitimate, authenticated driving messages pass all 6 pipeline stages.
Expected Output: PipelineDecision::VALID_FORWARD with accepted = true.
>>> RESULT: PASS

TEST 8: testInjectionAttack
Purpose: Verify gateway detects and rejects forged/spoofed CAN frames.
Expected Output: REJECT_HMAC_AUTHENTICATION_FAILED or REJECT_FRAME_VALIDATION_FAILED.
>>> RESULT: PASS

TEST 9: testReplayAttack
Purpose: Verify gateway detects and rejects replayed authentic CAN frame.
Expected Output: REJECT_REPLAY_DETECTED.
>>> RESULT: PASS

TEST 10: testDosTraffic
Purpose: Verify gateway ML anomaly engine detects high-frequency DoS flooding.
Expected Output: ML Anomaly detected (DOS_FLOOD_ANOMALY).
>>> RESULT: PASS

TEST 11: testFuzzyTraffic
Purpose: Verify gateway rejects random fuzzed CAN IDs and corrupted structures.
Expected Output: REJECT_FRAME_VALIDATION_FAILED.
>>> RESULT: PASS

TEST 12: testFalseDataTraffic
Purpose: Verify out-of-bounds speed (320 km/h) is rejected even with signed HMAC.
Expected Output: REJECT_FRAME_VALIDATION_FAILED with FALSE_DATA_MANIPULATION.
>>> RESULT: PASS

TEST 13: testGatewayDecisionLogic
Purpose: Verify gateway arbitration counters and comprehensive metrics tracking.
Expected Output: Metrics match exact frame totals.
>>> RESULT: PASS

============================================================
  TEST RESULTS SUMMARY
  Total Tests Executed : 13
  Total Tests Passed   : 13
  Total Tests Failed   : 0
============================================================
```

---

## 12. Current Software-Only Status & Future Hardware Requirements

### Current Status (Phase 1)
- **100% Software Simulation**: The complete defense pipeline, cryptographic authenticators, replay monitors, ML models, attack simulators, Ethernet forwarder, dashboard, and offline AWS loggers are fully functional in software.
- Zero physical hardware or external credentials required to build and verify.

### What Still Requires Physical Hardware (Phase 2 Roadmap)
1. **Raspberry Pi Pico 2 Microcontroller**: Needs physical deployment of bare-metal C++ firmware using the Raspberry Pi Pico SDK.
2. **Physical Sensors**:
   - Potentiometer (wired to Pico 2 ADC0 / GP26) for manual throttle simulation.
   - DS18B20 / Thermistor (wired to Pico 2 ADC1 / GP27) for physical temperature sensing.
   - SW-420 Vibration Sensor (wired to Pico 2 ADC2 / GP28) for mechanical shock sensing.
   - Optical / Hall-effect encoder (wired to GP15) for wheel pulse measurement.
3. **Dual MCP2515 CAN Controller Breakout Modules**: Need physical wiring over hardware SPI to both the Pico 2 and Raspberry Pi 4.
4. **Physical CAN Bus Physical Layer**: 120-Ohm terminated twisted-pair wiring connecting CAN_H and CAN_L between the two MCP2515 modules.
5. **Raspberry Pi 4 Gateway**: Requires flashing Raspberry Pi OS, configuring the `mcp2515-can0` devicetree overlay in `/boot/config.txt`, and bringing up SocketCAN (`can0`).
6. **Physical Ethernet Cable**: Direct connection between Raspberry Pi 4 Ethernet port and the dashboard workstation.
7. **Production AWS Cloud Deployment**: AWS IAM Access Keys and live S3 / CloudWatch permissions (currently running offline using `LocalMockAwsLogger`).

For detailed wiring schematics and pinouts, see [docs/hardware_integration_plan.md](docs/hardware_integration_plan.md).
