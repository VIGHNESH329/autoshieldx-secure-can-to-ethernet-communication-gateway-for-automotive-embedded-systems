#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "mcp2515.h"
#include "sha256_hmac.h"
#include "can_protocol.h"
#include "sensor_input.h"

#define HEARTBEAT_LED_PIN 25 // Standard Pico / Pico 2 onboard LED

int main(void) {
    // 1. Initialize Standard I/O over USB Serial (115200 baud)
    stdio_init_all();

    // 2. Initialize Onboard LED Indicator
    gpio_init(HEARTBEAT_LED_PIN);
    gpio_set_dir(HEARTBEAT_LED_PIN, GPIO_OUT);
    gpio_put(HEARTBEAT_LED_PIN, 1);

    // Give USB serial time to enumerate when connected to host PC
    sleep_ms(2000);

    printf("\n============================================================\n");
    printf("  AutoShieldX - Physical ECU Transmitter Firmware           \n");
    printf("  Hardware: Raspberry Pi Pico 2 (RP2350) + MCP2515 CAN SPI   \n");
    printf("  SPI0: MISO=GP16, CS=GP17, SCK=GP18, MOSI=GP19             \n");
    printf("  CAN Bitrate: 500 kbps (Normal Mode)                       \n");
    printf("  CAN ID: 0x100 (Telemetry) | 0x101 (Auth Container)        \n");
    printf("  Security: Full RFC 2104 HMAC-SHA256 (32 Bytes / 256 Bits) \n");
    printf("============================================================\n\n");

    // 3. Initialize MCP2515 CAN Controller
    // Standard breakout modules commonly use 8 MHz quartz crystals (set true).
    // If your breakout has a 16 MHz crystal, change parameter to false.
    bool mcp_ok = mcp2515_init(true);
    if (!mcp_ok) {
        printf("[Pico 2] Retrying MCP2515 init with 16 MHz crystal configuration...\n");
        mcp_ok = mcp2515_init(false);
    }

    if (!mcp_ok) {
        printf("[Pico 2] FATAL: MCP2515 initialization failed. Halting.\n");
        while (1) {
            gpio_put(HEARTBEAT_LED_PIN, 1);
            sleep_ms(100);
            gpio_put(HEARTBEAT_LED_PIN, 0);
            sleep_ms(100);
        }
    }

    // 4. Initialize Vehicle Telemetry Kinematics Subsystem
    sensor_subsystem_init();

    printf("[Pico 2] ECU armed. Beginning 20 Hz (50 ms) periodic authenticated CAN transmissions...\n\n");

    uint32_t sequence_number = 1;
    uint32_t frame_count = 0;
    vehicle_telemetry_t telemetry;

    while (1) {
        uint64_t start_time = time_us_64();

        // 1. Advance vehicle telemetry simulation
        sensor_subsystem_read(0.050, &telemetry);

        // 2. Encode into classic CAN 8-byte powertrain payload
        uint8_t payload[8];
        encode_powertrain_payload(
            telemetry.speed_kmh,
            telemetry.rpm,
            telemetry.throttle_pct,
            telemetry.temperature_c,
            sequence_number,
            payload);

        // 3. Construct 17-byte canonical authentication buffer:
        // [CAN_ID (4B BE)] | [Sequence (4B BE)] | [DLC (1B = 8)] | [Payload (8B)]
        uint8_t canonical_buf[17];
        build_canonical_buffer(CAN_ID_POWERTRAIN, sequence_number, payload, canonical_buf);

        // 4. Calculate full 32-byte (256-bit) HMAC-SHA256 authentication tag
        uint8_t hmac32[32];
        hmac_sha256(SHARED_SECRET, SHARED_SECRET_LEN, canonical_buf, sizeof(canonical_buf), hmac32);

        // 5. Transmit Telemetry Frame (CAN ID 0x100, DLC 8)
        static uint32_t tx_fail_count = 0;
        if (!mcp2515_send_frame(CAN_ID_POWERTRAIN, 8, payload)) {
            tx_fail_count++;
        }

        // Short inter-frame spacing for MCP2515 TX buffer clearance (~100 us)
        sleep_us(100);

        // 6. Transmit 6 Authentication Container Fragment Frames (CAN ID 0x101, DLC 8)
        uint8_t correlation_seq = (uint8_t)(sequence_number & 0xFF);

        for (uint8_t frag_idx = 0; frag_idx < AX_CTP_TOTAL_FRAGMENTS; ++frag_idx) {
            uint8_t auth_frame[8];
            // Byte 0: Header [frag_idx: 4 bits | total_frags: 4 bits]
            auth_frame[0] = (uint8_t)((frag_idx << 4) | (AX_CTP_TOTAL_FRAGMENTS & 0x0F));
            // Byte 1: Correlation sequence counter (low 8 bits)
            auth_frame[1] = correlation_seq;

            if (frag_idx < 5) {
                // Fragments 0..4 carry 6 bytes of HMAC each (total 30 bytes)
                memcpy(&auth_frame[2], &hmac32[frag_idx * 6], 6);
            } else {
                // Fragment 5 carries final 2 bytes of HMAC + High 16-bit sequence counter
                auth_frame[2] = hmac32[30];
                auth_frame[3] = hmac32[31];
                auth_frame[4] = (uint8_t)((sequence_number >> 24) & 0xFF);
                auth_frame[5] = (uint8_t)((sequence_number >> 16) & 0xFF);
                auth_frame[6] = 0x00; // Reserved
                auth_frame[7] = 0x00; // Reserved
            }

            if (!mcp2515_send_frame(CAN_ID_POWERTRAIN_AUTH, 8, auth_frame)) {
                tx_fail_count++;
            }
            sleep_us(100);
        }

        // Toggle Heartbeat LED
        gpio_put(HEARTBEAT_LED_PIN, (sequence_number % 10) < 5);

        frame_count++;
        sequence_number++;

        // Print technical log every 20 frames (1 second)
        if (frame_count % 20 == 0) {
            printf("[Pico 2 TX #%lu] Seq: %lu | Speed: %.1f km/h | RPM: %.0f | Temp: %.1f C | Drops: %lu | HMAC: %02x%02x%02x%02x...\n",
                   frame_count,
                   sequence_number - 1,
                   telemetry.speed_kmh,
                   telemetry.rpm,
                   telemetry.temperature_c,
                   tx_fail_count,
                   hmac32[0], hmac32[1], hmac32[2], hmac32[3]);
        }

        // Maintain strict 20 Hz transmission cadence (50 ms cycle)
        uint64_t elapsed_us = time_us_64() - start_time;
        if (elapsed_us < 50000) {
            sleep_us(50000 - elapsed_us);
        }
    }

    return 0;
}
