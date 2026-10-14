#include "autoshieldx/can/can_frame.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace autoshieldx {

std::string SecureCanMessage::serialize() const {
    // Delimited wire format for simulation:
    // CAN_ID|SEQ|TIMESTAMP|SPEED|RPM|TEMP|VIB|STEER|THROT|WHEEL|HEX_PAYLOAD|HMAC_TAG
    std::ostringstream oss;
    oss << can_id << "|"
        << sequence_number << "|"
        << timestamp_us << "|"
        << std::fixed << std::setprecision(2)
        << telemetry.speed_kmh << "|"
        << telemetry.rpm << "|"
        << telemetry.temperature_c << "|"
        << std::setprecision(4)
        << telemetry.vibration_g << "|"
        << std::setprecision(2)
        << telemetry.steering_deg << "|"
        << telemetry.throttle_pct << "|"
        << telemetry.wheel_speed_kmh << "|";

    // 8-byte payload in hex
    for (size_t i = 0; i < 8; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(payload[i]);
    }
    oss << "|" << hmac_tag;
    return oss.str();
}

bool SecureCanMessage::deserialize(const std::string& serialized, SecureCanMessage& out_msg) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(serialized);

    while (std::getline(iss, token, '|')) {
        tokens.push_back(token);
    }

    // If the trailing token (e.g. empty HMAC tag) is empty, std::getline reaches EOF on '|'
    if (!serialized.empty() && serialized.back() == '|' && tokens.size() == 11) {
        tokens.push_back("");
    }

    if (tokens.size() < 12) {
        return false;
    }

    try {
        out_msg.can_id = static_cast<uint32_t>(std::stoul(tokens[0]));
        out_msg.sequence_number = static_cast<uint32_t>(std::stoul(tokens[1]));
        out_msg.timestamp_us = std::stoull(tokens[2]);
        out_msg.telemetry.speed_kmh = std::stod(tokens[3]);
        out_msg.telemetry.rpm = std::stod(tokens[4]);
        out_msg.telemetry.temperature_c = std::stod(tokens[5]);
        out_msg.telemetry.vibration_g = std::stod(tokens[6]);
        out_msg.telemetry.steering_deg = std::stod(tokens[7]);
        out_msg.telemetry.throttle_pct = std::stod(tokens[8]);
        out_msg.telemetry.wheel_speed_kmh = std::stod(tokens[9]);

        // Decode hex payload (16 hex chars -> 8 bytes)
        const std::string& hex_payload = tokens[10];
        if (hex_payload.length() == 16) {
            for (size_t i = 0; i < 8; ++i) {
                std::string byte_str = hex_payload.substr(i * 2, 2);
                out_msg.payload[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            }
        }

        out_msg.hmac_tag = tokens[11];
        return true;
    } catch (...) {
        return false;
    }
}

std::string SecureCanMessage::toString() const {
    std::ostringstream oss;
    oss << "CAN ID: 0x" << std::hex << can_id << std::dec
        << " | Seq: " << sequence_number
        << " | Speed: " << telemetry.speed_kmh << " km/h"
        << " | RPM: " << telemetry.rpm
        << " | Temp: " << telemetry.temperature_c << " C"
        << " | Steer: " << telemetry.steering_deg << " deg"
        << " | Vib: " << telemetry.vibration_g << " G"
        << " | HMAC: " << (hmac_tag.empty() ? "[NONE]" : hmac_tag.substr(0, 16) + "...");
    return oss.str();
}

std::array<uint8_t, 8> CanPayloadCodec::encodePowertrain(
    double speed_kmh, double rpm, double throttle_pct, double temp_c, uint32_t seq)
{
    std::array<uint8_t, 8> data{};
    uint16_t speed_scaled = static_cast<uint16_t>(std::max(0.0, speed_kmh * 100.0));
    uint16_t rpm_scaled = static_cast<uint16_t>(std::max(0.0, rpm));
    uint8_t throttle_scaled = static_cast<uint8_t>(std::clamp(throttle_pct, 0.0, 100.0));
    uint8_t temp_scaled = static_cast<uint8_t>(std::clamp(temp_c + 40.0, 0.0, 255.0)); // -40C offset
    uint16_t seq_low = static_cast<uint16_t>(seq & 0xFFFF);

    data[0] = static_cast<uint8_t>((speed_scaled >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(speed_scaled & 0xFF);
    data[2] = static_cast<uint8_t>((rpm_scaled >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(rpm_scaled & 0xFF);
    data[4] = throttle_scaled;
    data[5] = temp_scaled;
    data[6] = static_cast<uint8_t>((seq_low >> 8) & 0xFF);
    data[7] = static_cast<uint8_t>(seq_low & 0xFF);
    return data;
}

bool CanPayloadCodec::decodePowertrain(
    const std::array<uint8_t, 8>& data,
    double& out_speed, double& out_rpm, double& out_throttle, double& out_temp, uint32_t& out_seq_low)
{
    uint16_t speed_scaled = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    uint16_t rpm_scaled   = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    uint8_t throttle_raw  = data[4];
    uint8_t temp_raw      = data[5];
    uint16_t seq_low      = (static_cast<uint16_t>(data[6]) << 8) | data[7];

    out_speed = static_cast<double>(speed_scaled) / 100.0;
    out_rpm = static_cast<double>(rpm_scaled);
    out_throttle = static_cast<double>(throttle_raw);
    out_temp = static_cast<double>(temp_raw) - 40.0;
    out_seq_low = seq_low;
    return true;
}

std::array<uint8_t, 8> CanPayloadCodec::encodeDynamics(
    double steering_deg, double wheel_speed_kmh, double vibration_g, uint32_t seq)
{
    std::array<uint8_t, 8> data{};
    uint16_t steer_scaled = static_cast<uint16_t>(std::clamp((steering_deg + 540.0) * 10.0, 0.0, 65535.0));
    uint16_t wheel_scaled = static_cast<uint16_t>(std::max(0.0, wheel_speed_kmh * 100.0));
    uint16_t vib_scaled   = static_cast<uint16_t>(std::clamp(vibration_g * 1000.0, 0.0, 65535.0));
    uint16_t seq_low      = static_cast<uint16_t>(seq & 0xFFFF);

    data[0] = static_cast<uint8_t>((steer_scaled >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(steer_scaled & 0xFF);
    data[2] = static_cast<uint8_t>((wheel_scaled >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(wheel_scaled & 0xFF);
    data[4] = static_cast<uint8_t>((vib_scaled >> 8) & 0xFF);
    data[5] = static_cast<uint8_t>(vib_scaled & 0xFF);
    data[6] = static_cast<uint8_t>((seq_low >> 8) & 0xFF);
    data[7] = static_cast<uint8_t>(seq_low & 0xFF);
    return data;
}

bool CanPayloadCodec::decodeDynamics(
    const std::array<uint8_t, 8>& data,
    double& out_steering, double& out_wheel_speed, double& out_vibration, uint32_t& out_seq_low)
{
    uint16_t steer_scaled = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    uint16_t wheel_scaled = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    uint16_t vib_scaled   = (static_cast<uint16_t>(data[4]) << 8) | data[5];
    uint16_t seq_low      = (static_cast<uint16_t>(data[6]) << 8) | data[7];

    out_steering = (static_cast<double>(steer_scaled) / 10.0) - 540.0;
    out_wheel_speed = static_cast<double>(wheel_scaled) / 100.0;
    out_vibration = static_cast<double>(vib_scaled) / 1000.0;
    out_seq_low = seq_low;
    return true;
}

} // namespace autoshieldx
