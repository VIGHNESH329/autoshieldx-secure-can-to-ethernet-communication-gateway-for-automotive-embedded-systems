/**
 * ============================================================================
 * AutoShieldX - Physical ECU Transmitter Firmware (Arduino 1)
 * Hardware: Arduino Uno R3 (ATmega328P) + MCP2515 CAN SPI Module + TJA1050
 * CAN Bus: 500 kbps (Normal Mode)
 * Protocol: AutoShieldX CAN Transport Protocol (AX-CTP)
 * CAN IDs: 0x100 (Powertrain Telemetry) | 0x101 (Auth Container)
 * Crypto: RFC 2104 HMAC-SHA256 (32 Bytes / 256 Bits)
 * ============================================================================
 * 
 * WIRING GUIDE (Arduino Uno R3 to MCP2515 Module):
 * ------------------------------------------------
 * MCP2515 VCC  --> Arduino 5V
 * MCP2515 GND  --> Arduino GND
 * MCP2515 CS   --> Arduino Pin 10 (or Pin 9 if using shield)
 * MCP2515 SO   --> Arduino Pin 12 (SPI MISO)
 * MCP2515 SI   --> Arduino Pin 11 (SPI MOSI)
 * MCP2515 SCK  --> Arduino Pin 13 (SPI SCK)
 * MCP2515 INT  --> Arduino Pin 2 (Optional)
 *
 * CAN BUS CONNECTIONS (ISO 11898-2):
 * ----------------------------------
 * CAN_H  --> Raspberry Pi MCP2515 CAN_H
 * CAN_L  --> Raspberry Pi MCP2515 CAN_L
 * J1 Jumper: Bridged (120-Ohm termination resistor enabled at both ends)
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include "mcp2515.h"
#include "sha256_hmac.h"
#include "can_protocol.h"

// --------------------------------------------------------------------------
// CONFIGURATION PARAMETERS
// --------------------------------------------------------------------------
#define CAN_CS_PIN         10    // MCP2515 SPI Chip Select Pin
#define STATUS_LED_PIN     13    // Arduino onboard LED indicator
#define POTENTIOMETER_PIN  A0    // Optional physical sensor on Analog Pin A0

// Set to 1 for 8 MHz crystal (most blue breakout boards marked '8.000')
// Set to 0 for 16 MHz crystal (shields marked '16.000')
#define MCP_CRYSTAL_8MHZ   1

// Transmission cadence: 20 Hz = 50ms interval (AutoShieldX standard)
#define TX_INTERVAL_MS     50

// Operational Modes
enum EcuTransmissionMode {
    MODE_AUTHENTICATED,   // Normal Secure Mode: 0x100 Telemetry + 6x 0x101 HMAC fragments
    MODE_UNAUTHENTICATED, // Demo Mode: 0x100 Telemetry only (triggers Gateway HMAC failure)
    MODE_CORRUPTED_HMAC   // Attack Demo: 0x100 + 0x101 with 1-bit corrupted HMAC tag
};

// Global state
MCP2515_Driver canBus(CAN_CS_PIN);
EcuTransmissionMode currentMode = MODE_AUTHENTICATED;
bool usePotentiometer = false;
uint32_t sequenceNumber = 1;
uint32_t totalFramesSent = 0;
uint32_t txFailures = 0;
unsigned long lastTxMillis = 0;

// Vehicle kinematics model variables
float vehicleSpeed = 0.0f;
float engineRpm = 850.0f;
float throttlePct = 0.0f;
float engineTemp = 75.0f;
float simulationTime = 0.0f;

// Advance simulated driving kinematics
void advanceKinematics(float dt) {
    if (usePotentiometer) {
        // Read physical potentiometer from A0 (0..1023)
        int potRaw = analogRead(POTENTIOMETER_PIN);
        throttlePct = (float)potRaw / 10.23f; // 0..100%
        vehicleSpeed = (throttlePct / 100.0f) * 160.0f; // 0..160 km/h
        engineRpm = 850.0f + (throttlePct / 100.0f) * 4500.0f;
        engineTemp = 75.0f + (throttlePct / 100.0f) * 20.0f;
    } else {
        // Automatic realistic drive-cycle simulation (acceleration, cruise, deceleration)
        simulationTime += dt;
        float cycle = fmod(simulationTime, 30.0f); // 30-second driving loop

        if (cycle < 8.0f) {
            // Acceleration phase
            float progress = cycle / 8.0f;
            throttlePct = 25.0f + progress * 50.0f;
            vehicleSpeed = progress * 95.0f;
            engineRpm = 1200.0f + progress * 3200.0f;
        } else if (cycle < 20.0f) {
            // Cruising phase (highway driving with subtle micro-fluctuations)
            throttlePct = 30.0f + sin(cycle * 1.5f) * 5.0f;
            vehicleSpeed = 95.0f + sin(cycle * 0.8f) * 3.0f;
            engineRpm = 2400.0f + sin(cycle * 0.8f) * 100.0f;
        } else if (cycle < 26.0f) {
            // Deceleration / coasting phase
            float progress = (cycle - 20.0f) / 6.0f;
            throttlePct = 0.0f;
            vehicleSpeed = 95.0f * (1.0f - progress * 0.8f);
            engineRpm = 2400.0f - progress * 1400.0f;
        } else {
            // Low-speed urban / idle recovery
            throttlePct = 10.0f;
            vehicleSpeed = 20.0f;
            engineRpm = 1100.0f;
        }

        // Engine temperature warm-up curve (stabilizes around 86°C)
        engineTemp = 82.0f + sin(cycle * 0.2f) * 4.0f;
    }

    // Sanity boundary clipping
    if (vehicleSpeed < 0.0f) vehicleSpeed = 0.0f;
    if (vehicleSpeed > 220.0f) vehicleSpeed = 220.0f;
    if (engineRpm < 800.0f) engineRpm = 800.0f;
    if (engineRpm > 7500.0f) engineRpm = 7500.0f;
}

void printHelpMenu() {
    Serial.println(F("\n============================================================"));
    Serial.println(F("  AutoShieldX Arduino 1 - Interactive Control Console        "));
    Serial.println(F("============================================================"));
    Serial.println(F("  's' : Secure Authenticated Mode (0x100 + 0x101 AX-CTP HMAC) [DEFAULT]"));
    Serial.println(F("  'u' : Unauthenticated Mode (0x100 only, tests Gateway drop)"));
    Serial.println(F("  'a' : Attack Mode (corrupts 1 byte of HMAC to test detection)"));
    Serial.println(F("  'p' : Toggle Potentiometer Input (Analog Pin A0)"));
    Serial.println(F("  'h' : Display this help menu"));
    Serial.println(F("============================================================\n"));
}

void setup() {
    // 1. Initialize Serial Console
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Give serial monitor time to attach

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    Serial.println(F("\n============================================================"));
    Serial.println(F("  AutoShieldX - Physical ECU Transmitter Firmware (Arduino 1)"));
    Serial.println(F("  Architecture: Arduino Uno R3 (ATmega328P) + MCP2515 CAN SPI"));
    Serial.print(F("  CAN Bitrate : 500 kbps (Normal Mode, "));
    Serial.print(MCP_CRYSTAL_8MHZ ? F("8 MHz") : F("16 MHz"));
    Serial.println(F(" Crystal)"));
    Serial.println(F("  CAN IDs     : 0x100 (Powertrain) | 0x101 (Auth Container)"));
    Serial.println(F("  Crypto      : Full RFC 2104 HMAC-SHA256 (32 Bytes / 256 Bits)"));
    Serial.println(F("============================================================\n"));

    // 2. Initialize MCP2515 Controller
    bool ok = canBus.begin(MCP_CRYSTAL_8MHZ == 1);
    if (!ok) {
        Serial.println(F("[MCP2515] Initializing with 8 MHz failed, attempting 16 MHz..."));
        ok = canBus.begin(false);
    }

    if (!ok) {
        Serial.println(F("[MCP2515] FATAL: Initialization failed! Check SPI wiring & 5V power. Halting."));
        while (true) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(100);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(100);
        }
    }

    Serial.println(F("[MCP2515] Controller armed in NORMAL mode. Beginning 20 Hz transmission..."));
    printHelpMenu();
    digitalWrite(STATUS_LED_PIN, LOW);
}

void loop() {
    // 1. Handle incoming serial interactive commands
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 's':
            case 'S':
                currentMode = MODE_AUTHENTICATED;
                Serial.println(F("\n>>> [MODE] SECURE AUTHENTICATED MODE ACTIVE (0x100 + 0x101 AX-CTP HMAC)"));
                break;
            case 'u':
            case 'U':
                currentMode = MODE_UNAUTHENTICATED;
                Serial.println(F("\n>>> [MODE] UNAUTHENTICATED MODE ACTIVE (Sending raw 0x100 without HMAC)"));
                Serial.println(F(">>> Expect Gateway to log: HMAC_AUTH_FAILED (Severity: CRITICAL)"));
                break;
            case 'a':
            case 'A':
                currentMode = MODE_CORRUPTED_HMAC;
                Serial.println(F("\n>>> [MODE] ATTACK INJECTION ACTIVE (Corrupting HMAC tag byte 0)"));
                Serial.println(F(">>> Expect Gateway to log: HMAC_AUTH_FAILED (Attack: INJECTION_SPOOFING)"));
                break;
            case 'p':
            case 'P':
                usePotentiometer = !usePotentiometer;
                Serial.print(F("\n>>> [SENSOR] Potentiometer on A0: "));
                Serial.println(usePotentiometer ? F("ENABLED (Turn knob to change speed/throttle)") : F("DISABLED (Drive cycle simulation)"));
                break;
            case 'h':
            case 'H':
                printHelpMenu();
                break;
            default:
                break;
        }
    }

    // 2. Non-blocking 20 Hz transmission loop (every 50 ms)
    unsigned long currentMillis = millis();
    if (currentMillis - lastTxMillis >= TX_INTERVAL_MS) {
        lastTxMillis = currentMillis;

        // Step A: Update vehicle telemetry model
        advanceKinematics(0.050f);

        // Step B: Encode into standard AutoShieldX 8-byte Powertrain CAN payload
        uint8_t payload[8];
        encode_powertrain_payload(
            vehicleSpeed,
            engineRpm,
            throttlePct,
            engineTemp,
            sequenceNumber,
            payload);

        // Step C: Build 17-byte canonical buffer:
        // [CAN_ID (4B BE)] | [Sequence (4B BE)] | [DLC (1B = 8)] | [Payload (8B)]
        uint8_t canonicalBuf[17];
        build_canonical_buffer(CAN_ID_POWERTRAIN, sequenceNumber, payload, canonicalBuf);

        // Step D: Calculate full 32-byte RFC 2104 HMAC-SHA256 authentication tag
        uint8_t hmac32[32];
        hmac_sha256(SHARED_SECRET, SHARED_SECRET_LEN, canonicalBuf, sizeof(canonicalBuf), hmac32);

        if (currentMode == MODE_CORRUPTED_HMAC) {
            hmac32[0] ^= 0xFF; // Flip all bits in byte 0 to simulate payload tampering / spoofing
        }

        // Step E: Transmit Telemetry Frame (CAN ID 0x100, DLC 8)
        if (!canBus.sendFrame(CAN_ID_POWERTRAIN, 8, payload)) {
            txFailures++;
        }
        totalFramesSent++;

        // Step F: If in Authenticated or Corrupted mode, transmit the 6 AX-CTP Auth Fragments (CAN ID 0x101)
        if (currentMode == MODE_AUTHENTICATED || currentMode == MODE_CORRUPTED_HMAC) {
            uint8_t correlationSeq = (uint8_t)(sequenceNumber & 0xFF);

            for (uint8_t fragIdx = 0; fragIdx < AX_CTP_TOTAL_FRAGMENTS; ++fragIdx) {
                uint8_t authFrame[8];
                // Byte 0: Header [frag_index (4 bits) | total_fragments (4 bits = 6)]
                authFrame[0] = (uint8_t)((fragIdx << 4) | (AX_CTP_TOTAL_FRAGMENTS & 0x0F));
                // Byte 1: Correlation sequence counter (low 8 bits)
                authFrame[1] = correlationSeq;

                if (fragIdx < 5) {
                    // Fragments 0..4 carry 6 bytes of HMAC each (total 30 bytes)
                    memcpy(&authFrame[2], &hmac32[fragIdx * 6], 6);
                } else {
                    // Fragment 5 carries final 2 bytes of HMAC + High 16-bit sequence counter
                    authFrame[2] = hmac32[30];
                    authFrame[3] = hmac32[31];
                    authFrame[4] = (uint8_t)((sequenceNumber >> 24) & 0xFF);
                    authFrame[5] = (uint8_t)((sequenceNumber >> 16) & 0xFF);
                    authFrame[6] = 0x00; // Reserved
                    authFrame[7] = 0x00; // Reserved
                }

                delayMicroseconds(100); // Inter-frame spacing for MCP2515 TX buffer clearance
                if (!canBus.sendFrame(CAN_ID_POWERTRAIN_AUTH, 8, authFrame)) {
                    txFailures++;
                }
                totalFramesSent++;
            }
        }

        // Toggle Status LED (blinks with activity)
        digitalWrite(STATUS_LED_PIN, (sequenceNumber % 10) < 5 ? HIGH : LOW);

        // Step G: Periodic formatted Serial Monitor output (once per second / every 20 cycles)
        if (sequenceNumber % 20 == 0) {
            Serial.print(F("[Arduino 1 TX] Seq: "));
            Serial.print(sequenceNumber);
            Serial.print(F(" | Spd: "));
            Serial.print(vehicleSpeed, 1);
            Serial.print(F(" km/h | RPM: "));
            Serial.print((int)engineRpm);
            Serial.print(F(" | Temp: "));
            Serial.print(engineTemp, 1);
            Serial.print(F(" C | Throt: "));
            Serial.print((int)throttlePct);
            Serial.print(F("% | Mode: "));
            if (currentMode == MODE_AUTHENTICATED) Serial.print(F("AUTH (0x100+0x101)"));
            else if (currentMode == MODE_UNAUTHENTICATED) Serial.print(F("UNAUTH (0x100 only)"));
            else Serial.print(F("CORRUPT (Attack)"));

            Serial.print(F(" | HMAC: "));
            for (int i = 0; i < 4; ++i) {
                if (hmac32[i] < 0x10) Serial.print('0');
                Serial.print(hmac32[i], HEX);
            }
            Serial.print(F("... | Drops: "));
            Serial.println(txFailures);
        }

        sequenceNumber++;
    }
}
