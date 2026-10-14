#pragma once

#include "autoshieldx/can/can_frame.hpp"
#include <vector>
#include <unordered_set>
#include <string>

namespace autoshieldx {

struct ValidationRules {
    std::unordered_set<uint32_t> allowed_can_ids{
        static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY),
        static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY),
        static_cast<uint32_t>(CanMessageId::ENVIRONMENT_TELEMETRY),
        static_cast<uint32_t>(CanMessageId::DIAGNOSTIC_ALERT)
    };

    double min_speed_kmh{0.0};
    double max_speed_kmh{250.0};

    double min_rpm{0.0};
    double max_rpm{8000.0};

    double min_temperature_c{-40.0};
    double max_temperature_c{125.0};

    double min_steering_deg{-540.0};
    double max_steering_deg{540.0};

    double min_wheel_speed_kmh{0.0};
    double max_wheel_speed_kmh{260.0};

    double min_vibration_g{0.0};
    double max_vibration_g{5.0};

    double min_throttle_pct{0.0};
    double max_throttle_pct{100.0};

    size_t expected_payload_length{8};

    // Maximum physically plausible difference between vehicle speed and wheel speed
    double max_wheel_speed_delta_kmh{35.0};
};

struct FrameValidationResult {
    bool is_valid{true};
    std::string failure_reason{};
    std::vector<std::string> violations{};
};

/**
 * @brief CAN Frame Syntactic & Physical Semantic Validation Engine
 *
 * Enforces strict boundary checks on all inbound vehicle telemetry frames:
 * 1. CAN ID Whitelist
 * 2. Payload Length (DLC)
 * 3. Sensor range boundaries (Speed, RPM, Temp, Steering, Wheel Speed, Vibration)
 * 4. Cross-signal physical consistency checks (e.g. wheel speed vs vehicle speed)
 */
class ValidationEngine {
public:
    explicit ValidationEngine(ValidationRules rules = ValidationRules());
    ~ValidationEngine() = default;

    /**
     * @brief Validate incoming SecureCanMessage against configured rules.
     */
    FrameValidationResult validateMessage(const SecureCanMessage& msg) const;

    /**
     * @brief Validate raw CAN frame (CAN ID and DLC).
     */
    FrameValidationResult validateRawFrame(const CanRawFrame& frame) const;

    // Rule mutation helpers
    bool loadConfig(const std::string& config_file_path);
    void setRules(const ValidationRules& rules) { rules_ = rules; }
    const ValidationRules& getRules() const { return rules_; }
    void addAllowedCanId(uint32_t id) { rules_.allowed_can_ids.insert(id); }

private:
    ValidationRules rules_;
};

} // namespace autoshieldx
