#include "autoshieldx/security/replay_protection.hpp"
#include <sstream>

namespace autoshieldx {

ReplayProtectionEngine::ReplayProtectionEngine(uint32_t max_allowed_gap)
    : max_allowed_gap_(max_allowed_gap)
{
}

ReplayCheckStatus ReplayProtectionEngine::validateSequence(uint32_t can_id, uint32_t sequence_number) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_checks_++;

    ReplayCheckStatus status;
    status.received_seq = sequence_number;

    // Check if this CAN ID is being seen for the first time
    if (!initialized_map_[can_id]) {
        initialized_map_[can_id] = true;
        last_sequence_map_[can_id] = sequence_number;
        accepted_frames_++;
        status.is_valid = true;
        status.expected_min_seq = sequence_number;
        status.gap = 0;
        status.reason = "Initial sequence accepted for session";
        return status;
    }

    uint32_t last_seq = last_sequence_map_[can_id];
    status.expected_min_seq = last_seq + 1;

    // 1. Strict Replay Check: Incoming sequence <= last accepted
    if (sequence_number <= last_seq) {
        replay_detections_++;
        status.is_valid = false;
        status.gap = 0;
        std::ostringstream oss;
        oss << "REPLAY DETECTED: Incoming sequence " << sequence_number
            << " is <= last accepted sequence " << last_seq;
        status.reason = oss.str();
        return status;
    }

    // 2. Sequence Gap Check: Incoming sequence jumped forward excessively
    uint32_t gap = sequence_number - last_seq;
    status.gap = gap;

    if (gap > max_allowed_gap_) {
        // Warning: potential packet loss or frame injection attack
        std::ostringstream oss;
        oss << "WARNING: Abnormal sequence gap of " << gap
            << " (last: " << last_seq << ", incoming: " << sequence_number << ")";
        status.reason = oss.str();
    } else {
        status.reason = "Sequence valid";
    }

    // Accept valid new sequence and advance monotonic counter
    last_sequence_map_[can_id] = sequence_number;
    accepted_frames_++;
    status.is_valid = true;
    return status;
}

uint32_t ReplayProtectionEngine::getLastAcceptedSequence(uint32_t can_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = last_sequence_map_.find(can_id);
    if (it != last_sequence_map_.end()) {
        return it->second;
    }
    return 0;
}

void ReplayProtectionEngine::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_sequence_map_.clear();
    initialized_map_.clear();
    total_checks_ = 0;
    replay_detections_ = 0;
    accepted_frames_ = 0;
}

void ReplayProtectionEngine::resetForCanId(uint32_t can_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_sequence_map_.erase(can_id);
    initialized_map_.erase(can_id);
}

} // namespace autoshieldx
