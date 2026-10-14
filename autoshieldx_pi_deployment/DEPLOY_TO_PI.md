# AutoShieldX — Raspberry Pi 4 Deployment & Hardware Guide

This document provides exact, step-by-step instructions to compile, configure, and execute the updated **AutoShieldX Secure CAN-to-Ethernet Gateway** on a **Raspberry Pi 4 Model B** running Raspberry Pi OS.

---

## 1. Target Hardware & Prerequisites

- **Host Device**: Raspberry Pi 4 Model B (2 GB, 4 GB, or 8 GB RAM)
- **Operating System**: Raspberry Pi OS 64-bit (Debian Bookworm or Bullseye)
- **CAN Hardware**: MCP2515 SPI CAN controller + TJA1050/MCP2551 CAN transceiver module
- **Transmitter Hardware**: Arduino Uno R3 running `firmware/arduino/arduino_ecu/arduino_ecu.ino`
- **Sensors**:
  - HC-SR04 Ultrasonic Distance Sensor (TRIG: Arduino D7, ECHO: Arduino D6)
  - Optional 10k Potentiometer on Arduino Pin A0
- **CAN Bus Speed**: 500 kbps (120 $\Omega$ termination resistors enabled on bus ends)

---

## 2. Raspberry Pi System Dependencies

On the Raspberry Pi 4, open a terminal and install the required build toolchain and SocketCAN utilities:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    can-utils \
    iproute2 \
    libssl-dev \
    python3 \
    python3-pip
```

---

## 3. Hardware Wiring & SocketCAN Configuration

### 3.1. MCP2515 to Raspberry Pi 4 GPIO Pinout

| MCP2515 Pin | Raspberry Pi 4 Pin | Physical Pin # | Function |
| :--- | :--- | :--- | :--- |
| **VCC** | 5V Power | Pin 2 or 4 | 5V DC Supply |
| **GND** | Ground | Pin 6, 9, 14, 20 | Ground Reference |
| **CS** | GPIO 8 (CE0) | Pin 24 | SPI Chip Select 0 |
| **SO / MISO**| GPIO 9 (MISO) | Pin 21 | SPI Master In Slave Out |
| **SI / MOSI**| GPIO 10 (MOSI)| Pin 19 | SPI Master Out Slave In |
| **SCK / CLK**| GPIO 11 (SCLK)| Pin 23 | SPI Clock |
| **INT** | GPIO 25 | Pin 22 | Interrupt Request |

### 3.2. Enable MCP2515 Kernel Overlay in `config.txt`

Edit the Raspberry Pi boot configuration:
- For **Raspberry Pi OS Bookworm**:
  ```bash
  sudo nano /boot/firmware/config.txt
  ```
- For **Raspberry Pi OS Bullseye**:
  ```bash
  sudo nano /boot/config.txt
  ```

Add the following lines at the bottom:
```ini
# AutoShieldX MCP2515 CAN Controller
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25
```
*(Note: If your MCP2515 crystal is labeled `16.000` MHz instead of `8.000` MHz, change `oscillator=8000000` to `oscillator=16000000`).*

Reboot the Raspberry Pi:
```bash
sudo reboot
```

### 3.3. Initialize the SocketCAN `can0` Interface

After rebooting, configure and bring up the `can0` interface at 500 kbps:

```bash
# Bring up can0 at 500 kbps with automatic bus-off recovery
sudo ip link set can0 up type can bitrate 500000 restart-ms 100
sudo ip link set can0 txqueuelen 1000

# Verify the interface status
ip -details link show can0
```

Expected output:
```
can0: <NOARP,UP,LOWER_UP,ECHO> mtu 16 qdisc pfifo_fast state UP mode DEFAULT ...
    link/can  promiscuity 0 minmtu 0 maxmtu 0 
    can state ERROR-ACTIVE (berr-counter tx 0 rx 0) restart-ms 100 
	  bitrate 500000 sample-point 0.750 
```

---

## 4. Building the Project on Raspberry Pi 4

Transfer the extracted `autoshieldx_pi_deployment` folder to the Raspberry Pi (e.g. `~/autoshieldx_pi_deployment`).

```bash
cd ~/autoshieldx_pi_deployment

# Create clean Linux build directory
mkdir -p build_pi
cd build_pi

# Configure with CMake (detects native GCC 12+, POSIX threads, and OpenSSL)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compile all targets using all 4 cores
make -j$(nproc)
```

### Compiled Executables (located in `build_pi/bin/`):
- `secure_gateway`: Real-time zero-trust CAN-to-Ethernet security gateway.
- `autoshieldx_tests`: Complete automated test suite (24 test vectors).
- `ecu_simulator`: Standalone C++ vehicle kinematics and telemetry simulator.
- `attack_injection`: CAN ID and payload forgery attack simulator.
- `attack_replay`: Captured authentic frame replay attack simulator.
- `attack_dos`: High-frequency CAN bus flooding simulator.
- `attack_fuzzy`: Random payload fuzzing simulator.
- `attack_false_data`: Semantic physics-violation manipulator.

---

## 5. Running the Test Suite on the Pi

Before connecting live vehicle hardware, verify that all cryptographic, reassembly, and arbitration functions pass natively on ARM:

```bash
cd ~/autoshieldx_pi_deployment
./build_pi/bin/autoshieldx_tests
```

**Expected Result**: All 24 tests execute and report:
```
============================================================
  TEST RESULTS SUMMARY
  Total Tests Executed : 24
  Total Tests Passed   : 24
  Total Tests Failed   : 0
============================================================
```

---

## 6. Shared-Secret Compatibility & Configuration

The shared cryptographic key is identical between the Arduino transmitter and the Raspberry Pi gateway:

- **Arduino Firmware Key (`can_protocol.h`)**:
  `"AutoShieldX_Secret_Key_2026!AUTO"` (32 ASCII bytes)
- **Gateway Config (`config/gateway_config.json`)**:
  `"shared_secret_hex": "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f"`

Both formats decode to the identical 256-bit binary HMAC-SHA256 secret.

---

## 7. Starting the Operations Dashboard & Gateway

### Step 1: Start the Dashboard Server
In **Terminal 1**:
```bash
cd ~/autoshieldx_pi_deployment
python3 dashboard/dashboard_server.py 8080 16000
```
- Web Dashboard UI: `http://127.0.0.1:8080` (or `http://<Raspberry_Pi_IP>:8080`)
- Telemetry UDP Listener: Port `16000`

### Step 2: Start the Secure Gateway in SocketCAN Mode
In **Terminal 2**:
```bash
cd ~/autoshieldx_pi_deployment

# Option A: Forward telemetry to local dashboard on Pi
sudo ./build_pi/bin/secure_gateway --socketcan --iface can0 --dest 127.0.0.1:16000

# Option B: Broadcast telemetry over Ethernet subnet to any PC / Android app
sudo ./build_pi/bin/secure_gateway --socketcan --iface can0 --broadcast

# Option C: Forward directly to presentation PC IP (e.g. 192.168.1.100)
sudo ./build_pi/bin/secure_gateway --socketcan --iface can0 --dest 192.168.1.100:16000
```

---

## 8. Hardware Verification & Live Demonstration

### 8.1. Inspect Inbound CAN Traffic on the Pi
In **Terminal 3**, run `candump` to see physical frames transmitted by the Arduino:
```bash
candump -tz can0
```

Expected live stream:
```
(000.000000)  can0  100   [8]  1A 5E 08 FC 1E 7E 00 01  # CAN ID 0x100 (Telemetry)
(000.001200)  can0  101   [8]  06 01 8F 4A 1C 09 E2 51  # CAN ID 0x101 (AX-CTP Fragment 0)
(000.002400)  can0  101   [8]  16 01 3B 7D 90 AA C4 F1  # CAN ID 0x101 (AX-CTP Fragment 1)
(000.003600)  can0  101   [8]  26 01 11 88 56 BD EE 34  # CAN ID 0x101 (AX-CTP Fragment 2)
(000.004800)  can0  101   [8]  36 01 9A C0 D1 45 77 23  # CAN ID 0x101 (AX-CTP Fragment 3)
(000.006000)  can0  101   [8]  46 01 8B 12 FF 0A 4E 61  # CAN ID 0x101 (AX-CTP Fragment 4)
(000.007200)  can0  101   [8]  56 01 7C 82 00 00 00 00  # CAN ID 0x101 (AX-CTP Fragment 5)
```

### 8.2. Interactive Demonstration Controls via Arduino Serial Monitor
Open Arduino Serial Monitor at **115200 baud**:
- **Normal Secure Driving**: Press `'s'` (Authenticates 0x100 with 6 fragments of 0x101; dashboard indicates `ACCEPTED_VALID`).
- **Unauthenticated Spoofing Test**: Press `'u'` (Sends only 0x100; gateway drops frame and dashboard logs `HMAC_AUTH_FAILED`).
- **Tampered Payload Attack**: Press `'a'` (Flips 1 bit in HMAC; gateway detects tampering and drops frame).
- **Physical Sensor Control**: Press `'p'` (Connects physical potentiometer on pin A0 to live speed and throttle).
- **Ultrasonic Distance Sensor**: Move an object in front of HC-SR04; sensor distance is reflected in telemetry and logged.
