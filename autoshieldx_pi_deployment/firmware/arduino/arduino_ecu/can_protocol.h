#ifndef ARDUINO_CAN_PROTOCOL_H
#define ARDUINO_CAN_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// AutoShieldX Standard CAN IDs
#define CAN_ID_POWERTRAIN      0x100
#define CAN_ID_POWERTRAIN_AUTH 0x101
#define AX_CTP_TOTAL_FRAGMENTS 6

// Shared authentication key (matches gateway_config.json: "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f")
static const uint8_t SHARED_SECRET[] = "AutoShieldX_Secret_Key_2026!AUTO";
#define SHARED_SECRET_LEN 32

/**
 * @brief Encode Powertrain Telemetry into standard 8-byte CAN payload:
 * - Bytes 0-1: Speed (km/h * 100, uint16 big-endian)
 * - Bytes 2-3: RPM (uint16 big-endian)
 * - Byte 4:   Throttle (0-100%, uint8)
 * - Byte 5:   Engine Temp (°C + 40, uint8)
 * - Bytes 6-7: Sequence Counter low 16 bits (uint16 big-endian)
 */
static inline void encode_powertrain_payload(
    float speed_kmh, float rpm, float throttle_pct, float temp_c, uint32_t seq,
    uint8_t payload[8])
{
    uint16_t speed_scaled = (uint16_t)(speed_kmh < 0.0f ? 0 : (speed_kmh * 100.0f));
    uint16_t rpm_scaled   = (uint16_t)(rpm < 0.0f ? 0 : rpm);
    uint8_t  throt_scaled = (uint8_t)(throttle_pct < 0.0f ? 0 : (throttle_pct > 100.0f ? 100 : throttle_pct));
    float    t_offset     = temp_c + 40.0f;
    uint8_t  temp_scaled  = (uint8_t)(t_offset < 0.0f ? 0 : (t_offset > 255.0f ? 255 : t_offset));
    uint16_t seq_low      = (uint16_t)(seq & 0xFFFF);

    payload[0] = (uint8_t)((speed_scaled >> 8) & 0xFF);
    payload[1] = (uint8_t)(speed_scaled & 0xFF);
    payload[2] = (uint8_t)((rpm_scaled >> 8) & 0xFF);
    payload[3] = (uint8_t)(rpm_scaled & 0xFF);
    payload[4] = throt_scaled;
    payload[5] = temp_scaled;
    payload[6] = (uint8_t)((seq_low >> 8) & 0xFF);
    payload[7] = (uint8_t)(seq_low & 0xFF);
}

/**
 * @brief Build 17-byte canonical buffer for HMAC-SHA256 authentication:
 * [CAN_ID (4B BE)] | [Sequence (4B BE)] | [DLC (1B = 8)] | [Payload (8B)]
 */
static inline size_t build_canonical_buffer(
    uint32_t can_id, uint32_t sequence_number, const uint8_t payload[8],
    uint8_t canonical_buf[17])
{
    // Big-endian CAN ID
    canonical_buf[0] = (uint8_t)((can_id >> 24) & 0xFF);
    canonical_buf[1] = (uint8_t)((can_id >> 16) & 0xFF);
    canonical_buf[2] = (uint8_t)((can_id >> 8) & 0xFF);
    canonical_buf[3] = (uint8_t)(can_id & 0xFF);

    // Big-endian 32-bit Sequence Number
    canonical_buf[4] = (uint8_t)((sequence_number >> 24) & 0xFF);
    canonical_buf[5] = (uint8_t)((sequence_number >> 16) & 0xFF);
    canonical_buf[6] = (uint8_t)((sequence_number >> 8) & 0xFF);
    canonical_buf[7] = (uint8_t)(sequence_number & 0xFF);

    // DLC = 8
    canonical_buf[8] = 8;

    // 8-byte payload
    memcpy(&canonical_buf[9], payload, 8);

    return 17;
}

#ifdef __cplusplus
}
#endif

#endif // ARDUINO_CAN_PROTOCOL_H
