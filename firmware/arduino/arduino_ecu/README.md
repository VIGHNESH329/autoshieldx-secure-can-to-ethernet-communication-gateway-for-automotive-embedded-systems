# AutoShieldX - Arduino 1 (ECU Transmitter) Firmware Guide

This firmware replaces the original Raspberry Pi Pico 2 traffic source with an **Arduino Uno R3** + **MCP2515 CAN SPI module** to transmit physical CAN bus telemetry into the AutoShieldX cybersecurity gateway.

---

## 1. Hardware Pinout & Wiring

### 1.1 Arduino Uno R3 to MCP2515 Module
| MCP2515 Module Pin | Arduino Uno R3 Pin | Function | Notes |
|---|---|---|---|
| **VCC** | **5V** | Power (5V DC) | MCP2515 transceiver (TJA1050) requires 5V |
| **GND** | **GND** | Ground | Common system ground |
| **CS** | **Pin 10** | SPI Chip Select | Configurable via `#define CAN_CS_PIN 10` (Pin 9 on some shields) |
| **SO (MISO)** | **Pin 12** | SPI MISO | Hardware SPI |
| **SI (MOSI)** | **Pin 11** | SPI MOSI | Hardware SPI |
| **SCK** | **Pin 13** | SPI SCK | Hardware SPI |
| **INT** | **Pin 2** | Interrupt | Optional |

### 1.2 Physical CAN Bus (ISO 11898-2)
| Arduino MCP2515 | Raspberry Pi 4 MCP2515 (can0) | Notes |
|---|---|---|
| **CAN_H** | **CAN_H** | Twisted pair signal |
| **CAN_L** | **CAN_L** | Twisted pair signal |
| **J1 Jumper** | **J1 Jumper** | **Ensure 120Ω termination resistor jumper is bridged at both ends!** |

### 1.3 Optional Physical Sensor
- Connect a **10k Potentiometer**:
  - Center pin -> Arduino **Analog Pin A0**
  - Left pin -> 5V
  - Right pin -> GND
  - Turn the knob to adjust speed and throttle in real-time during live demos! (Press `'p'` in Serial Monitor to toggle).

---

## 2. Crystal Oscillator Configuration (CRITICAL)

Check the metal crystal on your MCP2515 breakout board:
- If marked **8.000** (8 MHz, standard blue boards): Keep `#define MCP_CRYSTAL_8MHZ 1` in `arduino_ecu.ino`.
- If marked **16.000** (16 MHz, common on shields): Change to `#define MCP_CRYSTAL_8MHZ 0` in `arduino_ecu.ino`.

---

## 3. How to Upload to Arduino Uno R3

1. Open the Arduino IDE.
2. Go to **File -> Open** and select:
   `firmware/arduino/arduino_ecu.ino`
   *(All required driver files `mcp2515.h`, `mcp2515.cpp`, `sha256_hmac.h`, `sha256_hmac.c`, and `can_protocol.h` reside in the same folder with zero external library dependencies).*
3. Select **Tools -> Board -> Arduino Uno**.
4. Select your COM Port under **Tools -> Port**.
5. Click **Upload**.
6. Open **Tools -> Serial Monitor** and set baud rate to **115200**.

---

## 4. Interactive Serial Monitor Commands

Open the Serial Monitor at 115200 baud to view live transmission statistics or interactively test attack detection:

| Key | Mode / Function | Gateway & Dashboard Response |
|---|---|---|
| **`s`** | **Secure Authenticated Mode** (Default) | Full 0x100 + 0x101 AX-CTP HMAC sent. Gateway accepts with **`ACCEPTED_VALID`** (Green dials update on Dashboard). |
| **`u`** | **Unauthenticated Mode** | Only 0x100 sent without HMAC. Gateway flags **`HMAC_AUTH_FAILED`** (Red rejection alert on Dashboard). |
| **`a`** | **Attack Injection Mode** | 0x100 + 0x101 sent with 1-bit corrupted HMAC. Gateway detects spoofing and drops frame. |
| **`p`** | **Toggle Potentiometer Input** | Switches between simulated drive cycle and physical A0 potentiometer knob. |
| **`h`** | **Help Menu** | Prints interactive command reference. |

---

## 5. Verification on Raspberry Pi 4

### Step 1: Check physical frame reception with `candump`
```bash
candump can0
```
In authenticated mode, you will observe 7 frames per 50 ms cycle:
```
  can0  100   [8]  1A 5E 08 FC 1E 7E 00 01
  can0  101   [8]  06 01 8F 4A 1C 09 E2 51
  can0  101   [8]  16 01 3B 7D 90 AA C4 F1
  can0  101   [8]  26 01 11 88 56 BD EE 34
  can0  101   [8]  36 01 9A C0 D1 45 77 23
  can0  101   [8]  46 01 8B 12 FF 0A 4E 61
  can0  101   [8]  56 01 7C 82 00 00 00 00
```

### Step 2: Run Secure Gateway
```bash
sudo ./build_pi/secure_gateway --socketcan
```
The gateway reassembles all 7 frames, validates HMAC-SHA256, verifies sequence counter freshness, and forwards live JSON telemetry over Ethernet UDP port 16000 to the dashboard.
