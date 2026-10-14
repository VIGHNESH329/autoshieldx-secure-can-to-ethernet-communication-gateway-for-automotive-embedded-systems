#pragma once

#include "autoshieldx/common.hpp"
#include <unordered_map>
#include <cstdint>
#include <string>
#include <mutex>

namespace autoshieldx {

struct ReplayCheckStatus {
    bool is_valid{false};
    uint32_t expected_min_seq{0};
    uint32_t received_seq{0};
    uint32_t gap{0};
    std::string reason{};
};

/**
 * @brief Replay Protection & Freshness Verification Engine
 *
 * Implements monotonic sequence counter verification per CAN ID to prevent
 * malicious adversaries from capturing authentic signed CAN frames and
 * replaying them later to trick actuators (e.g. replaying an "accelerate" frame).
 *
 * Example:
 * Last Accepted: 10
 * Incoming: 11 -> ACCEPT (New last accepted: 11)
 * Incoming: 12 -> ACCEPT (New last accepted: 12)
 * Incoming: 10 -> REJECT as REPLAY (10 <= 12)
 * Incoming:  9 -> REJECT as REPLAY (9 <= 12)
 */
class ReplayProtectionEngine {
public:
    explicit ReplayProtectionEngine(uint32_t max_allowed_gap = 100);
    ~ReplayProtectionEngine() = default;

    /**
     * @brief Validate sequence number for a given CAN ID.
     * @param can_id CAN identifier
     * @param sequence_number Incoming sequence number
     * @return ReplayCheckStatus containing decision, gap, and explanation
     */
    ReplayCheckStatus validateSequence(uint32_t can_id, uint32_t sequence_number);

    /**
     * @brief Get the last accepted sequence number for a CAN ID.
     */
    uint32_t getLastAcceptedSequence(uint32_t can_id) const;

    /**
     * @brief Reset sequence tracking (e.g. on vehicle ignition cycle / key-on).
     */
    void reset();
    void resetForCanId(uint32_t can_id);

    // Operational statistics
    uint64_t getTotalChecks() const { return total_checks_; }
    uint64_t getReplayDetections() const { return replay_detections_; }
    uint64_t getAcceptedFrames() const { return accepted_frames_; }

private:
    uint32_t max_allowed_gap_;
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, uint32_t> last_sequence_map_;
    std::unordered_map<uint32_t, bool> initialized_map_;

    uint64_t total_checks_{0};
    uint64_t replay_detections_{0};
    uint64_t accepted_frames_{0};
};

} // namespace autoshieldx
