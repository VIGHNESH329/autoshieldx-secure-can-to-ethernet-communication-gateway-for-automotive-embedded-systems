#include "autoshieldx/validation/validation_engine.hpp"
#include <cmath>
#include <sstream>
#include <fstream>

namespace autoshieldx {

ValidationEngine::ValidationEngine(ValidationRules rules)
    : rules_(std::move(rules))
{
}

FrameValidationResult ValidationEngine::validateRawFrame(const CanRawFrame& frame) const {
    FrameValidationResult result;

    // 1. CAN ID Whitelist check
    if (rules_.allowed_can_ids.find(frame.can_id) == rules_.allowed_can_ids.end()) {
        result.is_valid = false;
        std::ostringstream oss;
        oss << "Unauthorized CAN ID: 0x" << std::hex << frame.can_id;
        result.violations.push_back(oss.str());
    }

    // 2. Data Length Code (DLC) check
    if (frame.can_dlc != rules_.expected_payload_length) {
        result.is_valid = false;
        std::ostringstream oss;
        oss << "Invalid Payload Length (DLC): " << static_cast<int>(frame.can_dlc)
            << " (expected " << rules_.expected_payload_length << ")";
        result.violations.push_back(oss.str());
    }

    if (!result.is_valid) {
        result.failure_reason = result.violations[0];
    }
    return result;
}

FrameValidationResult ValidationEngine::validateMessage(const SecureCanMessage& msg) const {
    FrameValidationResult result;

    // 1. CAN ID Whitelist check
    if (rules_.allowed_can_ids.find(msg.can_id) == rules_.allowed_can_ids.end()) {
        result.is_valid = false;
        std::ostringstream oss;
        oss << "Unauthorized CAN ID: 0x" << std::hex << msg.can_id;
        result.violations.push_back(oss.str());
    }

    // 2. Payload length check
    if (msg.payload.size() != rules_.expected_payload_length) {
        result.is_valid = false;
        std::ostringstream oss;
        oss << "Invalid Payload Length: " << msg.payload.size()
            << " bytes (expected " << rules_.expected_payload_length << ")";
        result.violations.push_back(oss.str());
    }

    // 3. Sequence number sanity (must not be 0 in active streams)
    if (msg.sequence_number == 0) {
        result.is_valid = false;
        result.violations.push_back("Invalid Sequence Number: 0");
    }

    // 4. Vehicle sensor boundary checks
    const auto& tel = msg.telemetry;

    // Specific validation based on CAN ID
    if (msg.can_id == static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY)) {
        if (tel.speed_kmh < rules_.min_speed_kmh || tel.speed_kmh > rules_.max_speed_kmh) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range Speed: " << tel.speed_kmh << " km/h (valid ["
                << rules_.min_speed_kmh << ", " << rules_.max_speed_kmh << "])";
            result.violations.push_back(oss.str());
        }

        if (tel.rpm < rules_.min_rpm || tel.rpm > rules_.max_rpm) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range RPM: " << tel.rpm << " (valid ["
                << rules_.min_rpm << ", " << rules_.max_rpm << "])";
            result.violations.push_back(oss.str());
        }

        if (tel.throttle_pct < rules_.min_throttle_pct || tel.throttle_pct > rules_.max_throttle_pct) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range Throttle: " << tel.throttle_pct << "% (valid ["
                << rules_.min_throttle_pct << ", " << rules_.max_throttle_pct << "])";
            result.violations.push_back(oss.str());
        }

        if (tel.temperature_c < rules_.min_temperature_c || tel.temperature_c > rules_.max_temperature_c) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range Engine Temperature: " << tel.temperature_c << " °C (valid ["
                << rules_.min_temperature_c << ", " << rules_.max_temperature_c << "])";
            result.violations.push_back(oss.str());
        }
    }
    else if (msg.can_id == static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY)) {
        if (tel.steering_deg < rules_.min_steering_deg || tel.steering_deg > rules_.max_steering_deg) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range Steering Angle: " << tel.steering_deg << " deg (valid ["
                << rules_.min_steering_deg << ", " << rules_.max_steering_deg << "])";
            result.violations.push_back(oss.str());
        }

        if (tel.wheel_speed_kmh < rules_.min_wheel_speed_kmh || tel.wheel_speed_kmh > rules_.max_wheel_speed_kmh) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range Wheel Speed: " << tel.wheel_speed_kmh << " km/h (valid ["
                << rules_.min_wheel_speed_kmh << ", " << rules_.max_wheel_speed_kmh << "])";
            result.violations.push_back(oss.str());
        }

        if (tel.vibration_g < rules_.min_vibration_g || tel.vibration_g > rules_.max_vibration_g) {
            result.is_valid = false;
            std::ostringstream oss;
            oss << "Out-of-range Vibration: " << tel.vibration_g << " G (valid ["
                << rules_.min_vibration_g << ", " << rules_.max_vibration_g << "])";
            result.violations.push_back(oss.str());
        }

        // Cross-signal physical consistency: vehicle speed vs wheel speed
        if (tel.speed_kmh > 0.0) {
            double delta = std::abs(tel.speed_kmh - tel.wheel_speed_kmh);
            if (delta > rules_.max_wheel_speed_delta_kmh) {
                result.is_valid = false;
                std::ostringstream oss;
                oss << "Physical Inconsistency: |Vehicle Speed (" << tel.speed_kmh
                    << ") - Wheel Speed (" << tel.wheel_speed_kmh << ")| = " << delta
                    << " km/h exceeds threshold (" << rules_.max_wheel_speed_delta_kmh << " km/h)";
                result.violations.push_back(oss.str());
            }
        }
    }

    if (!result.is_valid && !result.violations.empty()) {
        result.failure_reason = result.violations[0];
    }

    return result;
}

bool ValidationEngine::loadConfig(const std::string& config_file_path) {
    if (config_file_path.empty()) {
        return false;
    }
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string s = buffer.str();

    auto parseMinMax = [&](const std::string& key, double& min_val, double& max_val) {
        size_t pos = s.find("\"" + key + "\"");
        if (pos != std::string::npos) {
            size_t min_pos = s.find("\"min\"", pos);
            if (min_pos != std::string::npos && min_pos < pos + 100) {
                size_t colon = s.find(':', min_pos);
                if (colon != std::string::npos) {
                    try { min_val = std::stod(s.substr(colon + 1)); } catch (...) {}
                }
            }
            size_t max_pos = s.find("\"max\"", pos);
            if (max_pos != std::string::npos && max_pos < pos + 100) {
                size_t colon = s.find(':', max_pos);
                if (colon != std::string::npos) {
                    try { max_val = std::stod(s.substr(colon + 1)); } catch (...) {}
                }
            }
        }
    };

    parseMinMax("speed_kmh", rules_.min_speed_kmh, rules_.max_speed_kmh);
    parseMinMax("rpm", rules_.min_rpm, rules_.max_rpm);
    parseMinMax("temperature_c", rules_.min_temperature_c, rules_.max_temperature_c);
    parseMinMax("steering_angle_deg", rules_.min_steering_deg, rules_.max_steering_deg);
    parseMinMax("wheel_speed_kmh", rules_.min_wheel_speed_kmh, rules_.max_wheel_speed_kmh);
    parseMinMax("vibration_g", rules_.min_vibration_g, rules_.max_vibration_g);

    // Parse allowed CAN IDs if present
    size_t id_arr_pos = s.find("\"allowed_can_ids\"");
    if (id_arr_pos != std::string::npos) {
        size_t start = s.find('[', id_arr_pos);
        size_t end = s.find(']', id_arr_pos);
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string sub = s.substr(start + 1, end - start - 1);
            std::stringstream ss(sub);
            std::string item;
            std::unordered_set<uint32_t> parsed_ids;
            while (std::getline(ss, item, ',')) {
                // Trim whitespace
                item.erase(0, item.find_first_not_of(" \t\n\r"));
                item.erase(item.find_last_not_of(" \t\n\r") + 1);
                if (!item.empty()) {
                    try {
                        parsed_ids.insert(static_cast<uint32_t>(std::stoul(item)));
                    } catch (...) {}
                }
            }
            if (!parsed_ids.empty()) {
                rules_.allowed_can_ids = parsed_ids;
            }
        }
    }

    return true;
}

} // namespace autoshieldx
