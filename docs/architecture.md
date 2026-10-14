# AutoShieldX - System Architecture & Protocol Specification

## 1. Project Overview
**AutoShieldX** is a secure, defense-in-depth CAN-to-Ethernet gateway framework designed to protect in-vehicle Controller Area Network (CAN) communication against unauthorized message injection, replay playback, DoS bus flooding, fuzzing, and sensor manipulation attacks.

---

## 2. Hardware Topology (Target Architecture)

```
+---------------------------+
|   Raspberry Pi Pico 2     |  (ECU Simulator / Sensor Transmitter)
|   - Arm Cortex-M33 / RP2350
|   - ADC / GPIO Sensor Bus |
+-------------+-------------+
              | (SPI Interface)
+-------------v-------------+
|    MCP2515 CAN Module     |  (CAN Controller & TJA1050 Transceiver)
+-------------+-------------+
              | 
       === CAN BUS === (ISO 11898-2 Differential Twisted Pair: CAN_H / CAN_L at 500 kbps)
              |
+-------------v-------------+
|    MCP2515 CAN Module     |  (CAN Controller & TJA1050 Transceiver)
+-------------+-------------+
              | (SPI Interface)
+-------------v-------------+
|    Raspberry Pi 4 Model B |  (Secure Gateway)
|   - Linux SocketCAN (can0)|
|   - 6-Stage Defense Engine|
+-------------+-------------+
              | (Ethernet 100/1000 Mbps)
+-------------v-------------+
|    Operations Dashboard   |  (Vehicle Telemetry & Cyber Forensics)
+-------------+-------------+
              | (HTTPS / TLS 1.3)
+-------------v-------------+
|  AWS S3 & CloudWatch Logs |  (Cloud Telemetry Ingestion & Audit)
+---------------------------+
```

---

## 3. Phase 1 vs Phase 2 Scope

| System Component | Phase 1 (Current Status) | Phase 2 (Target Hardware) |
|---|---|---|
| **ECU Transmitter** | Software C++ Process with kinematic physics model | Raspberry Pi Pico 2 bare-metal C++ firmware |
| **Sensors** | `SimulatedSensorInput` (software equations) | Potentiometer (ADC0), Thermistor (ADC1), SW-420 (ADC2/GPIO), Optical Wheel Encoder |
| **CAN Bus Interface** | `SimulatedCanChannel` (UDP loopback 127.0.0.1:15000) | `SocketCanChannel` / SPI MCP2515 on Linux `can0` |
| **Gateway Processor** | Linux / Windows x86/ARM C++ Gateway | Raspberry Pi 4 Model B (Quad-core Cortex-A72) |
| **Ethernet Forwarding** | Localhost UDP stream to port 16000 | Physical Ethernet patch cable / vehicle Ethernet switch |
| **Dashboard** | Python HTTP & UDP server on `http://127.0.0.1:8080` | Real-time vehicle telematics console |
| **Cloud Logging** | `LocalMockAwsLogger` (local JSON batch serialization) | AWS SDK for C++ (CloudWatch Logs & S3 PutObject) |

---

## 4. CAN Frame & Telemetry Wire Format

### 4.1 CAN Identifiers (11-bit Standard IDs)
- `0x100` (256): Powertrain Telemetry (Speed, RPM, Throttle, Coolant Temperature, Sequence Counter Low)
- `0x200` (512): Dynamics Telemetry (Steering Angle, Wheel Speed, SW-420 Vibration, Sequence Counter Low)
- `0x300` (768): Body / Environmental Diagnostics
- `0x400` (1024): Security Gateway Status & Health

### 4.2 Raw CAN 2.0B Payload Layout (8 Bytes)
Matches standard `struct can_frame` from `<linux/can.h>`:
```
Powertrain (0x100):
Byte 0-1: Speed (km/h * 100, uint16 big-endian)
Byte 2-3: RPM (uint16 big-endian)
Byte 4:   Throttle (0-100%, uint8)
Byte 5:   Engine Temp (°C + 40 offset, uint8)
Byte 6-7: Sequence Counter Low 16 bits (uint16 big-endian)

Dynamics (0x200):
Byte 0-1: Steering Angle (deg * 10 + 5400, uint16 big-endian)
Byte 2-3: Wheel Speed (km/h * 100, uint16 big-endian)
Byte 4-5: Vibration (G * 1000, uint16 big-endian)
Byte 6-7: Sequence Counter Low 16 bits (uint16 big-endian)
```

---

## 5. Security Architecture
The gateway implements a strict 6-stage defense-in-depth pipeline:
1. **Frame Validation Engine**: Enforces CAN ID whitelist, payload length DLC=8, physical sensor bounds, and inter-signal consistency.
2. **Cryptographic HMAC-SHA256**: Authenticates sender identity and verifies data integrity.
3. **Replay Protection**: Enforces strictly increasing monotonic sequence numbers per CAN ID.
4. **Machine Learning Anomaly Engine**: Analyzes sliding-window message frequency (Hz), sequence continuity, and kinematic profiles.
5. **Decision Engine**: Arbitrates pass/drop decisions and classifies incident root causes.
6. **Ethernet Forwarder & Structured Forensics**: Pushes authorized frames to dashboard and writes structured audit logs.
