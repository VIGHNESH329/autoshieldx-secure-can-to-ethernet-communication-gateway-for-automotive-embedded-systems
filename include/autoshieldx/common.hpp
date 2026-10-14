#pragma once

#include <string>
#include <cstdint>
#include <chrono>

namespace autoshieldx {

// Standard CAN IDs used in AutoShieldX simulation
enum class CanMessageId : uint32_t {
    POWERTRAIN_TELEMETRY = 0x100, // 256: Speed, RPM, Throttle
    DYNAMICS_TELEMETRY   = 0x200, // 512: Steering Angle, Wheel Speeds
    ENVIRONMENT_TELEMETRY= 0x300, // 768: Engine Temp, Vibration
    DIAGNOSTIC_ALERT     = 0x400  // 1024: System Status, Diagnostics
};

// Gateway security decision result
enum class DecisionResult {
    VALID_FORWARD,
    REJECT_FRAME_VALIDATION_FAILED,
    REJECT_HMAC_AUTHENTICATION_FAILED,
    REJECT_REPLAY_DETECTED,
    REJECT_ML_ANOMALY_DETECTED,
    REJECT_MALFORMED_PAYLOAD
};

// Attack categories for simulation and classification
enum class AttackType {
    NONE = 0,
    INJECTION_SPOOFING = 1,
    REPLAY_ATTACK = 2,
    DOS_FLOODING = 3,
    FUZZY_TRAFFIC = 4,
    FALSE_DATA_MANIPULATION = 5
};

// Event severity levels
enum class Severity {
    INFO,
    WARNING,
    CRITICAL
};

inline std::string decisionToString(DecisionResult res) {
    switch (res) {
        case DecisionResult::VALID_FORWARD: return "VALID_FORWARD";
        case DecisionResult::REJECT_FRAME_VALIDATION_FAILED: return "REJECT_FRAME_VALIDATION_FAILED";
        case DecisionResult::REJECT_HMAC_AUTHENTICATION_FAILED: return "REJECT_HMAC_AUTHENTICATION_FAILED";
        case DecisionResult::REJECT_REPLAY_DETECTED: return "REJECT_REPLAY_DETECTED";
        case DecisionResult::REJECT_ML_ANOMALY_DETECTED: return "REJECT_ML_ANOMALY_DETECTED";
        case DecisionResult::REJECT_MALFORMED_PAYLOAD: return "REJECT_MALFORMED_PAYLOAD";
        default: return "UNKNOWN";
    }
}

inline std::string attackTypeToString(AttackType type) {
    switch (type) {
        case AttackType::NONE: return "NONE";
        case AttackType::INJECTION_SPOOFING: return "INJECTION_SPOOFING";
        case AttackType::REPLAY_ATTACK: return "REPLAY_ATTACK";
        case AttackType::DOS_FLOODING: return "DOS_FLOODING";
        case AttackType::FUZZY_TRAFFIC: return "FUZZY_TRAFFIC";
        case AttackType::FALSE_DATA_MANIPULATION: return "FALSE_DATA_MANIPULATION";
        default: return "UNKNOWN";
    }
}

inline std::string severityToString(Severity sev) {
    switch (sev) {
        case Severity::INFO: return "INFO";
        case Severity::WARNING: return "WARNING";
        case Severity::CRITICAL: return "CRITICAL";
        default: return "INFO";
    }
}

inline uint64_t getCurrentTimestampMicros() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

inline uint64_t getCurrentTimestampMillis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

} // namespace autoshieldx
