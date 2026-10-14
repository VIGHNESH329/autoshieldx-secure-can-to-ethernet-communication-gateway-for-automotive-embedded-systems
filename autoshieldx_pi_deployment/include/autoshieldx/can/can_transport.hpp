#pragma once

#include "autoshieldx/common.hpp"
#include "autoshieldx/can/can_frame.hpp"
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <chrono>

namespace autoshieldx {

/**
 * ============================================================================
 * AUTOSHIELDX CAN TRANSPORT PROTOCOL (AX-CTP)
 * Transport & Fragmentation Layer for Classic CAN 2.0B (MCP2515 / 8-byte DLC)
 * ============================================================================
 *
 * Framing Architecture:
 * 1. Telemetry Data Frame (CAN ID: Base ID, e.g. 0x100 Powertrain, 0x200 Dynamics)
 *    DLC = 8
 *    Payload: Standard 8-byte vehicle telemetry encoded by CanPayloadCodec.
 *    Bytes 0-1: Speed / Steering
 *    Bytes 2-3: RPM / Wheel Speed
 *    Byte 4:   Throttle / Vibration High
 *    Byte 5:   Engine Temp / Vibration Low
 *    Bytes 6-7: Sequence Counter Low 16 bits (BE)
 *
 * 2. Authentication Container Frames (CAN ID: Base ID + 1, e.g. 0x101, 0x201)
 *    DLC = 8
 *    Total Fragments = 6 (Indices 0..5)
 *    Byte 0: Header -> [Fragment Index: 4 bits | Total Fragments: 4 bits]
 *    Byte 1: Correlation ID -> Sequence Low 8 bits (matches Telemetry Byte 7)
 *    Bytes 2-7: HMAC Payload Chunk (up to 6 bytes per fragment):
 *      - Frag 0 (index 0): HMAC bytes [0..5]
 *      - Frag 1 (index 1): HMAC bytes [6..11]
 *      - Frag 2 (index 2): HMAC bytes [12..17]
 *      - Frag 3 (index 3): HMAC bytes [18..23]
 *      - Frag 4 (index 4): HMAC bytes [24..29]
 *      - Frag 5 (index 5): HMAC bytes [30..31] (2B) + Sequence High 16-bit (Bytes 4-5) + Pad (Bytes 6-7)
 *
 * Reassembly:
 * Reassembles complete 32-byte (256-bit) HMAC-SHA256 tag and full 32-bit sequence counter.
 * Constructs authentic SecureCanMessage and hands over to GatewayPipeline.
 */

constexpr uint8_t AX_CTP_TOTAL_FRAGMENTS = 6;
constexpr uint64_t AX_CTP_REASSEMBLY_TIMEOUT_US = 250000; // 250 ms timeout

struct ReassemblyContext {
    uint32_t base_can_id{0};
    uint32_t sequence_number{0};
    uint16_t seq_low16{0};
    uint16_t seq_high16{0};
    uint8_t correlation_seq8{0};
    bool telemetry_ready{false};
    std::array<uint8_t, 8> telemetry_payload{0};
    uint8_t received_fragment_mask{0}; // Bitmask of received fragment indices (0..5)
    std::array<uint8_t, 32> hmac_bytes{0};
    uint64_t first_frame_timestamp_us{0};
    uint64_t last_frame_timestamp_us{0};

    void reset() {
        base_can_id = 0;
        sequence_number = 0;
        seq_low16 = 0;
        seq_high16 = 0;
        correlation_seq8 = 0;
        telemetry_ready = false;
        telemetry_payload.fill(0);
        received_fragment_mask = 0;
        hmac_bytes.fill(0);
        first_frame_timestamp_us = 0;
        last_frame_timestamp_us = 0;
    }

    bool isComplete() const {
        // Complete when telemetry is received AND all 6 fragments (mask 0x3F = 0b00111111) are received
        return telemetry_ready && (received_fragment_mask == 0x3F);
    }
};

class CanTransportProtocol {
public:
    /**
     * @brief Fragment a high-level SecureCanMessage into classic CAN 2.0B 8-byte raw frames.
     * @param msg Input secure CAN message with 8-byte payload and 64-hex HMAC tag.
     * @param out_frames Vector of 7 CanRawFrame structs (1 Telemetry + 6 Auth Fragments).
     * @return true on successful fragmentation.
     */
    static bool fragmentMessage(const SecureCanMessage& msg, std::vector<CanRawFrame>& out_frames);

    /**
     * @brief Feed a raw CAN frame into reassembler state.
     * @param raw_frame Incoming raw CAN frame from SocketCAN / MCP2515.
     * @param out_msg Populated SecureCanMessage if a complete message was reassembled.
     * @return true if a complete message is ready; false if waiting for more fragments or ignored.
     */
    bool processIncomingFrame(const CanRawFrame& raw_frame, SecureCanMessage& out_msg);

    /**
     * @brief Check if any pending reassembly contexts have timed out (> 250 ms)
     *        waiting for authentication fragments.
     * @param out_msg Populated with unauthenticated SecureCanMessage if a context timed out.
     * @return true if an unauthenticated message was yielded for security evaluation.
     */
    bool checkTimeout(SecureCanMessage& out_msg);

    /**
     * @brief Reset reassembler internal state.
     */
    void reset();

private:
    std::unordered_map<uint32_t, ReassemblyContext> contexts_; // Keyed by base CAN ID
};

} // namespace autoshieldx
