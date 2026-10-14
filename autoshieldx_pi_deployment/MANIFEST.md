# AutoShieldX Raspberry Pi Deployment Manifest

This manifest documents every file included in the `autoshieldx_pi_deployment` package and explains why it is required for building, running, testing, or configuring the AutoShieldX system on a Raspberry Pi 4.

---

## 1. Build & Project Configuration

| File | Purpose / Justification |
| :--- | :--- |
| `CMakeLists.txt` | Core CMake build configuration. Detects Linux toolchain, POSIX Threads, OpenSSL, and compiles `autoshieldx_core`, `secure_gateway`, `ecu_simulator`, all 5 attack modules, and `autoshieldx_tests`. |
| `DEPLOY_TO_PI.md` | Deployment guide containing exact Linux dependencies, SocketCAN setup commands, compilation steps, wiring diagrams, and test instructions. |
| `README.md` | High-level system overview, protocol specifications, and architectural documentation. |
| `MANIFEST.md` | This file: comprehensive file catalog and justification for every included artifact. |

---

## 2. Gateway & Runtime Configuration (`config/`)

| File | Purpose / Justification |
| :--- | :--- |
| `config/gateway_config.json` | Master gateway configuration. Defines the shared HMAC secret (`4175746f...`), SocketCAN interface (`can0`), Ethernet forwarder target, validation limits, and logging settings. |
| `config/aws_config.json` | CloudWatch logging and telemetry exporter configuration for optional cloud telemetry ingestion. |

---

## 3. Operations Dashboard & Telemetry Collector (`dashboard/`)

| File | Purpose / Justification |
| :--- | :--- |
| `dashboard/dashboard_server.py` | Python HTTP and UDP telemetry collector. Hosts the live web UI on port 8080 and listens on UDP port 16000 for forwarded vehicle telemetry from `secure_gateway`. |
| `dashboard/index.html` | Operations Dashboard frontend. Displays vehicle telemetry gauges, zero-trust pipeline status, and live security forensics table (with replay attack control removed). |
| `dashboard/style.css` | Complete glassmorphic dark-mode CSS styling and responsive layout rules. |
| `dashboard/app.js` | Client-side JavaScript polling `/api/status` at 2 Hz to animate gauges and render incoming security alerts. |

---

## 4. C++ Header Files (`include/autoshieldx/`)

| File | Purpose / Justification |
| :--- | :--- |
| `include/autoshieldx/common.hpp` | Global data structures, `SecureCanMessage`, `PipelineDecision`, and enum definitions. |
| `include/autoshieldx/can/can_frame.hpp` | CAN frame structures and bit-level payload codec functions for powertrain and vehicle dynamics. |
| `include/autoshieldx/can/can_channel.hpp` | Abstract CAN channel interface (`ICanChannel`) and SocketCAN driver declaration for Linux `can0`. |
| `include/autoshieldx/can/can_transport.hpp` | AutoShieldX CAN Transport Protocol (AX-CTP) fragmentation and reassembly engine. |
| `include/autoshieldx/ecu/ecu_simulator.hpp` | Simulated vehicle ECU generating 20 Hz periodic telemetry. |
| `include/autoshieldx/ecu/sensor_interface.hpp` | Sensor input abstraction (`ISensorInput`, `SimulatedSensorInput`, and `RealSensorInput`). |
| `include/autoshieldx/security/crypto_hmac.hpp` | RFC 2104 compliant HMAC-SHA256 authentication engine supporting both OpenSSL and native fallback. |
| `include/autoshieldx/security/replay_protection.hpp` | Monotonic sequence number validator and sliding-window replay attack defense engine. |
| `include/autoshieldx/validation/validation_engine.hpp` | Syntactic DLC, sequence sanity, and semantic physical sensor boundary validation rules. |
| `include/autoshieldx/ml/ml_detector.hpp` | Lightweight embedded machine-learning heuristic intrusion detector for DoS, sequence gaps, and vibration spikes. |
| `include/autoshieldx/gateway/security_pipeline.hpp` | Core 5-stage zero-trust gateway arbitration pipeline coordinator. |
| `include/autoshieldx/gateway/gateway_logger.hpp` | Thread-safe local file and console forensic event logger. |
| `include/autoshieldx/networking/ethernet_forwarder.hpp` | UDP Ethernet transmission engine forwarding validated telemetry to the dashboard. |
| `include/autoshieldx/networking/aws_logger.hpp` | Optional AWS CloudWatch telemetry and security incident log exporter. |

---

## 5. C++ Source Files (`src/`)

| File | Purpose / Justification |
| :--- | :--- |
| `src/can/can_frame.cpp` | Codec implementation for packing and unpacking 8-byte CAN payloads. |
| `src/can/can_channel.cpp` | Linux SocketCAN raw socket implementation for binding to `can0` via `sockaddr_can`. |
| `src/can/can_transport.cpp` | Multi-frame reassembly logic for AX-CTP fragment reassembly (0x101 authentication frames). |
| `src/ecu/ecu_simulator.cpp` | Software ECU simulator logic for bench testing without Arduino hardware. |
| `src/ecu/sensor_interface.cpp` | Vehicle kinematics simulation model with periodic behavioral-anomaly generation. |
| `src/ecu/main_ecu.cpp` | Entrypoint for the standalone `ecu_simulator` executable. |
| `src/security/crypto_hmac.cpp` | Cryptographic HMAC-SHA256 computation and constant-time verification. |
| `src/security/replay_protection.cpp` | Monotonic counter validation and replay detection implementation. |
| `src/validation/validation_engine.cpp` | Syntactic and physical plausibility boundary check implementation. |
| `src/ml/ml_detector.cpp` | Rule-based and heuristic ML inference engine evaluating kinematic and frequency features. |
| `src/gateway/gateway_logger.cpp` | Logging subsystem implementation writing to `logs/autoshieldx_security.log`. |
| `src/gateway/security_pipeline.cpp` | 5-Stage security pipeline implementation executing all defense layers. |
| `src/gateway/main_gateway.cpp` | Entrypoint for `secure_gateway`, handling CLI arguments (`--socketcan`, `--broadcast`, `--dest`) and event loops. |
| `src/networking/ethernet_forwarder.cpp` | Serialization of validated frames into JSON and UDP dispatch to port 16000. |
| `src/networking/aws_logger.cpp` | AWS CloudWatch log formatting implementation. |

---

## 6. Laboratory Attack Simulators (`attacks/`)

| File | Purpose / Justification |
| :--- | :--- |
| `attacks/injection/attack_injection.cpp` | Simulates spoofed CAN frames on `can0` to demonstrate HMAC rejection. |
| `attacks/replay/attack_replay.cpp` | Replays captured valid frames to demonstrate sequence violation rejection. |
| `attacks/dos/attack_dos.cpp` | Floods `can0` with high-frequency frames to demonstrate ML DoS detection. |
| `attacks/fuzzy/attack_fuzzy.cpp` | Sends randomized payloads and CAN IDs to test syntactic validation defense. |
| `attacks/false_data/attack_false_data.cpp` | Sends validly signed out-of-bounds sensor values to demonstrate semantic validation. |

---

## 7. Automated Test Suite (`tests/`)

| File | Purpose / Justification |
| :--- | :--- |
| `tests/test_autoshieldx.cpp` | Complete 24-vector automated unit and integration test suite. Verifies HMAC RFC vectors, replay detection, AX-CTP reassembly, and boundary rules on the Raspberry Pi. |

---

## 8. Machine Learning Artifacts (`ml/inference/`)

| File | Purpose / Justification |
| :--- | :--- |
| `ml/inference/model_weights.json` | Pre-trained decision boundary weights and thresholds for embedded ML detection. |
| `ml/inference/ml_detector.py` | Python reference implementation of the behavioral anomaly detector. |

---

## 9. Arduino Firmware (`firmware/arduino/`)

| File | Purpose / Justification |
| :--- | :--- |
| `firmware/arduino/arduino_ecu.ino` | Standalone Arduino sketchbook sketch with simulated kinematics and HC-SR04 ultrasonic sensor support. |
| `firmware/arduino/arduino_ecu/arduino_ecu.ino` | Main Arduino firmware implementation for the Arduino Uno R3 transmitter. |
| `firmware/arduino/arduino_ecu/can_protocol.h` | Shared protocol constants, CAN IDs (`0x100`, `0x101`), AX-CTP headers, and shared secret. |
| `firmware/arduino/arduino_ecu/mcp2515.h` | Low-level MCP2515 register definitions and SPI driver header. |
| `firmware/arduino/arduino_ecu/mcp2515.cpp` | SPI register configuration and CAN frame transmission implementation. |
| `firmware/arduino/arduino_ecu/sha256_hmac.h` | Ultra-lightweight SHA-256 and HMAC C library header optimized for AVR (ATmega328P). |
| `firmware/arduino/arduino_ecu/sha256_hmac.c` | Standard C implementation of SHA-256 and HMAC for Arduino with zero dynamic memory allocation. |
| `firmware/arduino/arduino_ecu/README.md` | Flashing instructions, pinout diagram, and serial monitor interactive commands. |

---

## 10. Documentation (`docs/`)

| File | Purpose / Justification |
| :--- | :--- |
| `docs/architecture.md` | High-level system architecture and data-flow diagrams. |
| `docs/hardware_integration_plan.md` | Detailed hardware test setup, oscilloscope/candump observations, and verification plan. |
