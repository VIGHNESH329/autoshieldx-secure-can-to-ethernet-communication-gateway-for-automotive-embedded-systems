#pragma once

#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/validation/validation_engine.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include "autoshieldx/security/replay_protection.hpp"
#include "autoshieldx/ml/ml_detector.hpp"
#include "autoshieldx/gateway/gateway_logger.hpp"
#include "autoshieldx/networking/ethernet_forwarder.hpp"
#include <memory>
#include <string>

namespace autoshieldx {

struct PipelineDecision {
    DecisionResult result{DecisionResult::VALID_FORWARD};
    bool accepted{false};
    std::string stage_failed{};
    std::string reason{};
    AttackType inferred_attack{AttackType::NONE};
    double ml_anomaly_score{0.0};
    uint64_t processing_latency_us{0};
};

/**
 * @brief Secure Gateway Decision Pipeline
 *
 * Implements the 6-stage cybersecurity defense-in-depth pipeline:
 *
 * Receive Frame
 *      |
 * 1. Frame Validation (Syntactic/Range/Physical Boundaries)
 *      |
 * 2. Cryptographic HMAC-SHA256 Authentication (Zero-Trust Identity)
 *      |
 * 3. Freshness & Monotonic Replay Protection (Anti-Playback)
 *      |
 * 4. ML Behavioral Anomaly Detection (Frequency/Flow Profiling)
 *      |
 * 5. Decision Arbitration Engine
 *      |
 *      +---> Valid   -> Forward over Ethernet to Dashboard
 *      +---> Invalid -> Reject + Structured Forensic Log
 */
class GatewayPipeline {
public:
    GatewayPipeline(
        const std::string& shared_secret,
        std::shared_ptr<ValidationEngine> validator,
        std::shared_ptr<ReplayProtectionEngine> replay_engine,
        std::shared_ptr<IMlAnomalyDetector> ml_detector,
        std::shared_ptr<GatewayLogger> logger,
        std::shared_ptr<IEthernetForwarder> forwarder);

    GatewayPipeline(
        const std::vector<uint8_t>& secret_bytes,
        std::shared_ptr<ValidationEngine> validator,
        std::shared_ptr<ReplayProtectionEngine> replay_engine,
        std::shared_ptr<IMlAnomalyDetector> ml_detector,
        std::shared_ptr<GatewayLogger> logger,
        std::shared_ptr<IEthernetForwarder> forwarder);

    ~GatewayPipeline() = default;

    /**
     * @brief Process an inbound CAN message through the complete pipeline.
     */
    PipelineDecision processFrame(const SecureCanMessage& msg);

    // Metrics and counters
    uint64_t getTotalFrames() const { return total_frames_; }
    uint64_t getAcceptedFrames() const { return accepted_frames_; }
    uint64_t getRejectedFrames() const { return rejected_frames_; }
    uint64_t getHmacFailures() const { return hmac_failures_; }
    uint64_t getReplayFailures() const { return replay_failures_; }
    uint64_t getValidationFailures() const { return validation_failures_; }
    uint64_t getMlAnomalyCount() const { return ml_anomalies_; }

    void setPermissiveMode(bool enable) { permissive_mode_ = enable; }
    bool isPermissiveMode() const { return permissive_mode_; }

private:
    bool permissive_mode_{false};
    std::string shared_secret_;
    std::vector<uint8_t> secret_bytes_;

    std::shared_ptr<ValidationEngine> validator_;
    std::shared_ptr<ReplayProtectionEngine> replay_engine_;
    std::shared_ptr<IMlAnomalyDetector> ml_detector_;
    std::shared_ptr<GatewayLogger> logger_;
    std::shared_ptr<IEthernetForwarder> forwarder_;

    uint64_t total_frames_{0};
    uint64_t accepted_frames_{0};
    uint64_t rejected_frames_{0};
    uint64_t hmac_failures_{0};
    uint64_t replay_failures_{0};
    uint64_t validation_failures_{0};
    uint64_t ml_anomalies_{0};
};

} // namespace autoshieldx
