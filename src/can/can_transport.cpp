#include "autoshieldx/can/can_transport.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include <cstring>
#include <iostream>

namespace autoshieldx {

bool CanTransportProtocol::fragmentMessage(const SecureCanMessage& msg, std::vector<CanRawFrame>& out_frames) {
    out_frames.clear();
    out_frames.reserve(1 + AX_CTP_TOTAL_FRAGMENTS);

    uint64_t ts = (msg.timestamp_us > 0) ? msg.timestamp_us : getCurrentTimestampMicros();

    // 1. Frame 0: Telemetry Data PDU (CAN ID: msg.can_id, DLC: 8)
    CanRawFrame data_frame{};
    data_frame.can_id = msg.can_id;
    data_frame.can_dlc = 8;
    std::memcpy(data_frame.data, msg.payload.data(), 8);
    data_frame.timestamp_us = ts;
    out_frames.push_back(data_frame);

    // Convert HMAC hex string to 32 raw bytes
    std::vector<uint8_t> hmac_bytes;
    if (msg.hmac_tag.length() >= 64) {
        hmac_bytes = HmacSha256::hexToBytes(msg.hmac_tag.substr(0, 64));
    }
    if (hmac_bytes.size() < 32) {
        hmac_bytes.resize(32, 0x00);
    }

    uint8_t correlation_seq = static_cast<uint8_t>(msg.sequence_number & 0xFF);
    uint32_t auth_can_id = msg.can_id + 1; // Dedicated Auth CAN ID paired with base ID

    // 2. Frames 1..6: Authentication Container Fragments
    for (uint8_t frag_idx = 0; frag_idx < AX_CTP_TOTAL_FRAGMENTS; ++frag_idx) {
        CanRawFrame auth_frame{};
        auth_frame.can_id = auth_can_id;
        auth_frame.can_dlc = 8;
        auth_frame.timestamp_us = ts;

        // Byte 0: Header [frag_index: 4 bits | total_frags: 4 bits]
        auth_frame.data[0] = static_cast<uint8_t>((frag_idx << 4) | (AX_CTP_TOTAL_FRAGMENTS & 0x0F));
        // Byte 1: Correlation ID (Sequence Low 8 bits)
        auth_frame.data[1] = correlation_seq;

        if (frag_idx < 5) {
            // Fragments 0..4 carry 6 bytes each (total 30 bytes)
            std::memcpy(&auth_frame.data[2], &hmac_bytes[frag_idx * 6], 6);
        } else {
            // Fragment 5 carries last 2 bytes of HMAC + High 16 bits of Sequence Number
            auth_frame.data[2] = hmac_bytes[30];
            auth_frame.data[3] = hmac_bytes[31];
            auth_frame.data[4] = static_cast<uint8_t>((msg.sequence_number >> 24) & 0xFF);
            auth_frame.data[5] = static_cast<uint8_t>((msg.sequence_number >> 16) & 0xFF);
            auth_frame.data[6] = 0x00; // Reserved
            auth_frame.data[7] = 0x00; // Reserved
        }

        out_frames.push_back(auth_frame);
    }

    return true;
}

bool CanTransportProtocol::processIncomingFrame(const CanRawFrame& raw_frame, SecureCanMessage& out_msg) {
    uint64_t now_us = getCurrentTimestampMicros();

    // Check if this frame is an Auth Fragment (Odd CAN ID matching Base ID + 1)
    bool is_auth_frame = ((raw_frame.can_id & 0x01) == 0x01);

    if (is_auth_frame) {
        uint32_t base_id = raw_frame.can_id - 1;
        if (raw_frame.can_dlc < 8) {
            return false;
        }

        bool is_known_base_id = (
            base_id == static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY) ||
            base_id == static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY) ||
            base_id == static_cast<uint32_t>(CanMessageId::ENVIRONMENT_TELEMETRY) ||
            base_id == static_cast<uint32_t>(CanMessageId::DIAGNOSTIC_ALERT)
        );
        if (!is_known_base_id) {
            // Forward rogue auth frame directly so ValidationEngine catches unauthorized ID
            out_msg.can_id = raw_frame.can_id;
            out_msg.sequence_number = 0;
            out_msg.timestamp_us = (raw_frame.timestamp_us > 0) ? raw_frame.timestamp_us : now_us;
            std::memcpy(out_msg.payload.data(), raw_frame.data, 8);
            out_msg.hmac_tag = "";
            return true;
        }

        uint8_t frag_index = static_cast<uint8_t>((raw_frame.data[0] >> 4) & 0x0F);
        uint8_t total_frags = static_cast<uint8_t>(raw_frame.data[0] & 0x0F);
        uint8_t correlation_seq = raw_frame.data[1];

        if (total_frags != AX_CTP_TOTAL_FRAGMENTS || frag_index >= AX_CTP_TOTAL_FRAGMENTS) {
            return false;
        }

        auto& ctx = contexts_[base_id];
        ctx.base_can_id = base_id;

        // If context timed out or correlation sequence diverged, reset reassembly context
        if (ctx.telemetry_ready && ctx.correlation_seq8 != correlation_seq) {
            ctx.reset();
            ctx.base_can_id = base_id;
        }

        if (ctx.first_frame_timestamp_us > 0 && (now_us - ctx.first_frame_timestamp_us) > AX_CTP_REASSEMBLY_TIMEOUT_US) {
            ctx.reset();
            ctx.base_can_id = base_id;
        }

        if (ctx.first_frame_timestamp_us == 0) {
            ctx.first_frame_timestamp_us = (raw_frame.timestamp_us > 0) ? raw_frame.timestamp_us : now_us;
        }
        ctx.correlation_seq8 = correlation_seq;
        ctx.last_frame_timestamp_us = now_us;

        // Store HMAC chunk
        if (frag_index < 5) {
            std::memcpy(&ctx.hmac_bytes[frag_index * 6], &raw_frame.data[2], 6);
        } else {
            ctx.hmac_bytes[30] = raw_frame.data[2];
            ctx.hmac_bytes[31] = raw_frame.data[3];
            ctx.seq_high16 = (static_cast<uint16_t>(raw_frame.data[4]) << 8) | raw_frame.data[5];
        }

        ctx.received_fragment_mask |= static_cast<uint8_t>(1 << frag_index);

        if (ctx.isComplete()) {
            out_msg.can_id = ctx.base_can_id;
            out_msg.sequence_number = (static_cast<uint32_t>(ctx.seq_high16) << 16) | ctx.seq_low16;
            out_msg.timestamp_us = ctx.first_frame_timestamp_us;
            out_msg.payload = ctx.telemetry_payload;
            out_msg.hmac_tag = HmacSha256::bytesToHex(ctx.hmac_bytes.data(), 32);

            // Decode telemetry fields into out_msg.telemetry
            if (out_msg.can_id == static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY)) {
                uint32_t dummy_seq = 0;
                CanPayloadCodec::decodePowertrain(
                    out_msg.payload,
                    out_msg.telemetry.speed_kmh,
                    out_msg.telemetry.rpm,
                    out_msg.telemetry.throttle_pct,
                    out_msg.telemetry.temperature_c,
                    dummy_seq);
            } else if (out_msg.can_id == static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY)) {
                uint32_t dummy_seq = 0;
                CanPayloadCodec::decodeDynamics(
                    out_msg.payload,
                    out_msg.telemetry.steering_deg,
                    out_msg.telemetry.wheel_speed_kmh,
                    out_msg.telemetry.vibration_g,
                    dummy_seq);
            }

            ctx.reset();
            return true;
        }
        return false;
    }

    // Telemetry Data PDU (Even CAN ID, e.g. 0x100, 0x200)
    uint32_t base_id = raw_frame.can_id;

    // Check if this is an unauthorized/rogue CAN ID (e.g. 0x666 or non-standard)
    bool is_known_telemetry_id = (
        base_id == static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY) ||
        base_id == static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY) ||
        base_id == static_cast<uint32_t>(CanMessageId::ENVIRONMENT_TELEMETRY) ||
        base_id == static_cast<uint32_t>(CanMessageId::DIAGNOSTIC_ALERT)
    );

    if (!is_known_telemetry_id) {
        // Direct pass-through of rogue/injected frame so ValidationEngine can reject it
        out_msg.can_id = base_id;
        out_msg.sequence_number = 0;
        out_msg.timestamp_us = (raw_frame.timestamp_us > 0) ? raw_frame.timestamp_us : now_us;
        std::memcpy(out_msg.payload.data(), raw_frame.data, 8);
        out_msg.hmac_tag = ""; // Missing / unauthenticated
        return true;
    }

    auto& ctx = contexts_[base_id];

    // If a previous telemetry frame was received on this ID but its authentication fragments
    // never completed before this new telemetry frame arrived:
    bool previous_incomplete = ctx.telemetry_ready && !ctx.isComplete();
    SecureCanMessage pending_unauth_msg;

    if (previous_incomplete) {
        // Construct an unauthenticated message with missing HMAC so Gateway Security Pipeline
        // can inspect, validate physical limits, and flag the missing authentication tag!
        pending_unauth_msg.can_id = ctx.base_can_id;
        pending_unauth_msg.sequence_number = (static_cast<uint32_t>(ctx.seq_high16) << 16) | ctx.seq_low16;
        pending_unauth_msg.timestamp_us = ctx.first_frame_timestamp_us;
        pending_unauth_msg.payload = ctx.telemetry_payload;
        pending_unauth_msg.hmac_tag = ""; // Missing / unauthenticated

        if (pending_unauth_msg.can_id == static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY)) {
            uint32_t dummy_seq = 0;
            CanPayloadCodec::decodePowertrain(
                pending_unauth_msg.payload,
                pending_unauth_msg.telemetry.speed_kmh,
                pending_unauth_msg.telemetry.rpm,
                pending_unauth_msg.telemetry.throttle_pct,
                pending_unauth_msg.telemetry.temperature_c,
                dummy_seq);
        } else if (pending_unauth_msg.can_id == static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY)) {
            uint32_t dummy_seq = 0;
            CanPayloadCodec::decodeDynamics(
                pending_unauth_msg.payload,
                pending_unauth_msg.telemetry.steering_deg,
                pending_unauth_msg.telemetry.wheel_speed_kmh,
                pending_unauth_msg.telemetry.vibration_g,
                dummy_seq);
        }
    }

    ctx.reset();
    ctx.base_can_id = base_id;
    ctx.telemetry_ready = true;
    std::memcpy(ctx.telemetry_payload.data(), raw_frame.data, 8);

    // Extract sequence low 16 bits from payload (standard AutoShieldX layout)
    ctx.seq_low16 = (static_cast<uint16_t>(raw_frame.data[6]) << 8) | raw_frame.data[7];
    ctx.correlation_seq8 = raw_frame.data[7];
    ctx.first_frame_timestamp_us = (raw_frame.timestamp_us > 0) ? raw_frame.timestamp_us : now_us;
    ctx.last_frame_timestamp_us = now_us;

    if (previous_incomplete) {
        out_msg = pending_unauth_msg;
        return true; // Yield unauthenticated frame for gateway inspection!
    }

    return false; // Waiting for authentication container fragments
}

bool CanTransportProtocol::checkTimeout(SecureCanMessage& out_msg) {
    uint64_t now_us = getCurrentTimestampMicros();
    for (auto& pair : contexts_) {
        auto& ctx = pair.second;
        if (ctx.telemetry_ready && !ctx.isComplete() && ctx.first_frame_timestamp_us > 0) {
            if ((now_us - ctx.first_frame_timestamp_us) > AX_CTP_REASSEMBLY_TIMEOUT_US) {
                out_msg.can_id = ctx.base_can_id;
                out_msg.sequence_number = (static_cast<uint32_t>(ctx.seq_high16) << 16) | ctx.seq_low16;
                out_msg.timestamp_us = ctx.first_frame_timestamp_us;
                out_msg.payload = ctx.telemetry_payload;
                out_msg.hmac_tag = "";

                if (out_msg.can_id == static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY)) {
                    uint32_t dummy_seq = 0;
                    CanPayloadCodec::decodePowertrain(
                        out_msg.payload,
                        out_msg.telemetry.speed_kmh,
                        out_msg.telemetry.rpm,
                        out_msg.telemetry.throttle_pct,
                        out_msg.telemetry.temperature_c,
                        dummy_seq);
                } else if (out_msg.can_id == static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY)) {
                    uint32_t dummy_seq = 0;
                    CanPayloadCodec::decodeDynamics(
                        out_msg.payload,
                        out_msg.telemetry.steering_deg,
                        out_msg.telemetry.wheel_speed_kmh,
                        out_msg.telemetry.vibration_g,
                        dummy_seq);
                }

                ctx.reset();
                return true;
            }
        }
    }
    return false;
}

void CanTransportProtocol::reset() {
    contexts_.clear();
}

} // namespace autoshieldx
