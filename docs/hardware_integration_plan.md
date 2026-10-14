# AutoShieldX - Real Hardware Integration Guide

This guide details the complete hardware-software integration of **Arduino 1 (Arduino Uno R3)** / **Raspberry Pi Pico 2** + **MCP2515 CAN transceivers** + **Raspberry Pi 4 Gateway** with the AutoShieldX Cybersecurity Defense-in-Depth Pipeline and Live Monitoring Dashboard.

---

## 1. Physical Architecture & Wiring

```
[ Arduino 1 (Uno R3) / Pico 2 ]
       │ SPI (CS:D10, MOSI:D11, MISO:D12, SCK:D13)
[ MCP2515 #1 Transceiver ]
       │ CAN_H / CAN_L (500 kbps, 120Ω Terminated Twisted Pair)
[ MCP2515 #2 Transceiver ]
       │ SPI0 (MISO:GPIO9, CS:GPIO8, SCK:GPIO11, MOSI:GPIO10, INT:GPIO25)
[ Raspberry Pi 4 Gateway ]
       │ Linux SocketCAN (can0) -> AutoShieldX Security Pipeline
       │ Ethernet Forwarder (UDP 16000)
[ Dashboard (HTTP 8080) & AWS CloudWatch (/autoshieldx/telemetry) ]
```

### 1.1 Arduino Uno R3 (Arduino 1) to MCP2515 Wiring
| MCP2515 Pin | Arduino Uno R3 Pin | Function / Description |
|---|---|---|
| **VCC** | **5V** | 5V DC Power (TJA1050 transceiver requires 5V) |
| **GND** | **GND** | System Ground |
| **CS** | **Pin 10** | SPI Chip Select (configurable in sketch) |
| **SO (MISO)** | **Pin 12** | SPI MISO |
| **SI (MOSI)** | **Pin 11** | SPI MOSI |
| **SCK** | **Pin 13** | SPI SCK |
| **INT** | **Pin 2** | Interrupt (Optional) |
| **Analog In** | **Pin A0** | Optional: Connect 10k Potentiometer for physical knob control |

### 1.2 Raspberry Pi 4 Gateway to MCP2515 #2 Wiring
| MCP2515 Pin | Raspberry Pi 4 Physical Pin | GPIO / Function |
|---|---|---|
| **VCC** | Pin 2 or Pin 4 | 5V DC Power |
| **GND** | Pin 6, 9, 14, 20, or 25 | Ground |
| **CS** | Pin 24 | GPIO 8 (SPI0 CE0) |
| **SO (MISO)** | Pin 21 | GPIO 9 (SPI0 MISO) |
| **SI (MOSI)** | Pin 19 | GPIO 10 (SPI0 MOSI) |
| **SCK** | Pin 23 | GPIO 11 (SPI0 SCLK) |
| **INT** | Pin 22 | GPIO 25 (Hardware Interrupt) |

### 1.3 Physical CAN Bus (ISO 11898-2)
- Connect **CAN_H** of Arduino MCP2515 to **CAN_H** of Raspberry Pi MCP2515.
- Connect **CAN_L** of Arduino MCP2515 to **CAN_L** of Raspberry Pi MCP2515.
- Ensure **120-Ohm termination resistors** (J1 jumper) are bridged at both ends of the bus.

---

## 2. Root Cause Diagnosis: Why Dashboard Was Previously Dark

When Arduino 1 was initially transmitting, `candump can0` showed frames with ID `100`, but the dashboard did not update. Here is the verified technical explanation:

1. **Missing Authentication Fragments (AX-CTP)**:
   - AutoShieldX implements the **AX-CTP (AutoShieldX CAN Transport Protocol)** for classic 8-byte CAN frames.
   - A single message consists of:
     - **Frame 0 (CAN ID 0x100)**: 8-byte powertrain telemetry (speed, RPM, throttle, temp, sequence).
     - **Frames 1..6 (CAN ID 0x101)**: 6 authentication container fragments carrying the 32-byte (256-bit) RFC 2104 HMAC-SHA256 signature.
   - When Arduino 1 was only sending standalone 0x100 frames, the gateway's reassembler (`CanTransportProtocol`) stored the frame and waited for the 0x101 authentication fragments.
   - In the previous code, subsequent 0x100 frames reset the context without yielding the unauthenticated message to `pipeline.processFrame()`. Thus, zero messages were evaluated, zero logs were generated, and zero UDP packets were forwarded to the dashboard!

2. **Resolution Applied**:
   - **Gateway Resiliency**: `CanTransportProtocol` now flushes incomplete/standalone frames upon receiving the next telemetry frame or after timeout (250 ms) with an empty HMAC tag. The gateway pipeline now inspects these frames, verifies physical sensor boundaries, flags `HMAC_AUTH_FAILED` (Severity: CRITICAL), logs the intrusion event, and alerts the dashboard immediately!
   - **Permissive Mode**: The gateway supports `--permissive` (or `"permissive_mode": true`), allowing unauthenticated frames to be monitored and displayed on the dashboard with a warning badge.
   - **Arduino 1 Authenticated Firmware**: The new firmware in `firmware/arduino/arduino_ecu.ino` implements full RFC 2104 HMAC-SHA256 and transmits both CAN ID `0x100` and the 6 fragments on `0x101`, enabling 100% green `ACCEPTED_VALID` telemetry on the dashboard!

---

## 3. Step-by-Step Hardware Demonstration

### Step 3.1: Flash Arduino 1
1. Open `firmware/arduino/arduino_ecu.ino` in the Arduino IDE.
2. Verify crystal frequency:
   - If your MCP2515 module crystal is **8 MHz** (marked `8.000`): Keep `#define MCP_CRYSTAL_8MHZ 1`.
   - If your crystal is **16 MHz** (marked `16.000`): Change to `#define MCP_CRYSTAL_8MHZ 0`.
3. Select board **Arduino Uno**, choose the COM port, and click **Upload**.
4. Open the Serial Monitor at **115200 baud**. You will see:
   ```
   [MCP2515] Initialized successfully in NORMAL mode at 500 kbps (8 MHz crystal).
   [Arduino 1 TX] Seq: 20 | Spd: 45.2 km/h | RPM: 2150 | Temp: 82.4 C | Throt: 40% | Mode: AUTH (0x100+0x101) | HMAC: a1b2c3d4... | Drops: 0
   ```

### Step 3.2: Verify on Raspberry Pi with `candump`
```bash
candump can0
```
Output confirms all 7 physical frames arriving every 50 ms (20 Hz):
```
  can0  100   [8]  1A 5E 08 FC 1E 7E 00 01
  can0  101   [8]  06 01 8F 4A 1C 09 E2 51
  can0  101   [8]  16 01 3B 7D 90 AA C4 F1
  can0  101   [8]  26 01 11 88 56 BD EE 34
  can0  101   [8]  36 01 9A C0 D1 45 77 23
  can0  101   [8]  46 01 8B 12 FF 0A 4E 61
  can0  101   [8]  56 01 7C 82 00 00 00 00
```

### Step 3.3: Start Python Dashboard & Telemetry Collector
On the dashboard host (Windows or Raspberry Pi):
```bash
python dashboard/dashboard_server.py
```
- Web UI: `http://127.0.0.1:8080/`
- Telemetry UDP Port: `16000`
- AWS CloudWatch Logs target: `us-east-1` -> `/autoshieldx/telemetry`

### Step 3.4: Start Secure Gateway
On Raspberry Pi 4:
```bash
# If dashboard is running on Windows PC (e.g., 192.168.1.50):
sudo ./build_pi/secure_gateway --socketcan --dest 192.168.1.50:16000

# Or broadcast across local subnet:
sudo ./build_pi/secure_gateway --socketcan --broadcast

# Or if running dashboard locally on Raspberry Pi:
sudo ./build_pi/secure_gateway --socketcan --dest 127.0.0.1:16000
```

### Step 3.5: Live Demonstration Scenarios
1. **Normal Driving (Authenticated)**:
   - Arduino sends 0x100 + 0x101.
   - Gateway console displays: `[ETHERNET] JSON SENT bytes=301`.
   - Dashboard gauges (Speed, RPM, Temp, Vibration) animate in real time with green `ACCEPTED_VALID`.
2. **Interactive Attack Injection**:
   - In Arduino Serial Monitor, type `'u'` (Unauthenticated Mode).
   - Arduino sends only 0x100.
   - Dashboard instantly registers: **Rejected Messages: +1**, **HMAC Failures: +1**, red security alert `HMAC_AUTH_FAILED (UNAUTHORIZED SENDER)`.
   - Type `'a'` (Attack Corrupted Mode): Injects flipped bit into HMAC tag. Gateway detects payload tampering.
   - Type `'s'` (Secure Mode): Restores full authenticated operation.
3. **Physical Sensor Control**:
   - In Arduino Serial Monitor, type `'p'`.
   - Rotate the 10k potentiometer on Pin A0: vehicle speed and RPM dials respond in real time to the physical knob!
