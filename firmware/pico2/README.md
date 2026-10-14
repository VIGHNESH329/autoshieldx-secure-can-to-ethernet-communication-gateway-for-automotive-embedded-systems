# AutoShieldX - Raspberry Pi Pico 2 ECU Firmware

Bare-metal C firmware for the **Raspberry Pi Pico 2 (RP2350)** acting as the authenticated vehicle Electronic Control Unit (ECU).

---

## 1. Physical Hardware Wiring

The Pico 2 interfaces with the MCP2515 CAN controller breakout module over hardware **SPI0**:

| MCP2515 Pin | Pico 2 Physical Pin | RP2350 Function | Notes |
|-------------|---------------------|-----------------|-------|
| **VCC**     | Pin 39 (VSYS) or Pin 40 (VBUS) | 5V DC Power | MCP2515 transceiver (TJA1050) requires 5V |
| **GND**     | Pin 38 or Pin 3     | Ground          | Common Ground with bus |
| **CS**      | Pin 22 (GP17)       | GPIO Output     | Active-Low Chip Select |
| **SO (MISO)** | Pin 21 (GP16)     | SPI0 RX         | Master In, Slave Out |
| **SCK**     | Pin 24 (GP18)       | SPI0 SCK        | SPI Clock (10 MHz) |
| **SI (MOSI)** | Pin 25 (GP19)     | SPI0 TX         | Master Out, Slave In |
| **INT**     | Pin 26 (GP20)       | GPIO Interrupt  | Optional hardware interrupt |

### Bus Connection:
- Connect **CAN_H** of Pico 2 MCP2515 to **CAN_H** of Raspberry Pi 4 MCP2515.
- Connect **CAN_L** of Pico 2 MCP2515 to **CAN_L** of Raspberry Pi 4 MCP2515.
- Verify **120-Ohm termination resistors** (J1 jumper) are bridged on both MCP2515 boards.

---

## 2. Compilation Instructions

### Prerequisites
Install the Raspberry Pi Pico SDK and ARM GCC toolchain:
```bash
# Ubuntu / Raspberry Pi OS / Debian:
sudo apt-get update
sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git

# Clone Pico SDK (if not already installed)
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
export PICO_SDK_PATH=~/pico-sdk
```

### Build Steps
```bash
cd firmware/pico2
mkdir build && cd build

# For Pico 2 (RP2350):
cmake -DPICO_BOARD=pico2 -DPICO_SDK_PATH=$PICO_SDK_PATH ..
make -j$(nproc)
```

The build produces:
- `autoshieldx_pico2_ecu.uf2` (Drag-and-drop flashable binary)
- `autoshieldx_pico2_ecu.elf`
- `autoshieldx_pico2_ecu.bin`

---

## 3. Flashing the Pico 2

1. Disconnect the Pico 2 from USB.
2. Hold down the white **BOOTSEL** button on the Pico 2 board.
3. Plug the USB cable into your computer while holding **BOOTSEL**.
4. Release the button after 2 seconds. The Pico 2 will mount as a mass-storage drive named `RP2350` (or `RPI-RP2`).
5. Copy/drag `autoshieldx_pico2_ecu.uf2` onto the mounted drive.
6. The Pico 2 will immediately reboot and begin executing the firmware.
7. The onboard LED will pulse rhythmically as the heartbeat indicator.

---

## 4. Verification over USB Serial

Open a serial terminal at **115200 baud** to view live diagnostic output:
```bash
# Linux / macOS:
minicom -b 115200 -o -D /dev/ttyACM0
# or
screen /dev/ttyACM0 115200

# Windows:
# Use PuTTY or Tera Term on the detected COM port at 115200 baud.
```

Expected diagnostic output:
```
============================================================
  AutoShieldX - Physical ECU Transmitter Firmware           
  Hardware: Raspberry Pi Pico 2 (RP2350) + MCP2515 CAN SPI   
  SPI0: MISO=GP16, CS=GP17, SCK=GP18, MOSI=GP19             
  CAN Bitrate: 500 kbps (Normal Mode)                       
  CAN ID: 0x100 (Telemetry) | 0x101 (Auth Container)        
  Security: Full RFC 2104 HMAC-SHA256 (32 Bytes / 256 Bits) 
============================================================

[MCP2515] Initialized successfully in NORMAL mode at 500 kbps (8 MHz crystal).
[Pico 2] ECU armed. Beginning 20 Hz (50 ms) periodic authenticated CAN transmissions...

[Pico 2 TX #20] Seq: 20 | Speed: 68.4 km/h | RPM: 3140 | Temp: 86.2 C | Throttle: 27% | Steer: 1.2 deg | Vib: 0.114 G | HMAC Tag: 8f4a1c09...
[Pico 2 TX #40] Seq: 40 | Speed: 72.1 km/h | RPM: 3220 | Temp: 86.5 C | Throttle: 30% | Steer: 2.8 deg | Vib: 0.122 G | HMAC Tag: b40e872d...
```

---

## 5. Security & Framing Protocol (AX-CTP)

1. **CAN ID 0x100 (Powertrain Telemetry Data PDU)**:
   - 8-byte payload:
     - `Byte 0-1`: Speed * 100 (uint16 BE)
     - `Byte 2-3`: RPM (uint16 BE)
     - `Byte 4`:   Throttle (0-100%, uint8)
     - `Byte 5`:   Engine Temp + 40 (uint8)
     - `Byte 6-7`: Monotonic Sequence Counter Low 16 bits (uint16 BE)

2. **CAN ID 0x101 (Authentication Container PDUs)**:
   - 6 consecutive fragments carrying the 32-byte RFC 2104 HMAC-SHA256 tag and sequence correlation:
     - `Byte 0`: `[Fragment Index (4b) | Total Fragments = 6 (4b)]`
     - `Byte 1`: Correlation ID (`sequence_number & 0xFF`, locks with Frame 0x100)
     - `Bytes 2-7`: HMAC-SHA256 data chunks (32 bytes total across the 6 frames)
