#include "autoshieldx/gateway/security_pipeline.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace autoshieldx {

static std::string formatIsoTimestamp(uint64_t epoch_ms) {
    std::time_t t = static_cast<std::time_t>(epoch_ms / 1000);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

GatewayPipeline::GatewayPipeline(
    const std::string& shared_secret,
    std::shared_ptr<ValidationEngine> validator,
    std::shared_ptr<ReplayProtectionEngine> replay_engine,
    std::shared_ptr<IMlAnomalyDetector> ml_detector,
    std::shared_ptr<GatewayLogger> logger,
    std::shared_ptr<IEthernetForwarder> forwarder)
    : shared_secret_(shared_secret)
    , validator_(std::move(validator))
    , replay_engine_(std::move(replay_engine))
    , ml_detector_(std::move(ml_detector))
    , logger_(std::move(logger))
    , forwarder_(std::move(forwarder))
{
    secret_bytes_ = HmacSha256::parseKey(shared_secret_);
}

GatewayPipeline::GatewayPipeline(
    const std::vector<uint8_t>& secret_bytes,
    std::shared_ptr<ValidationEngine> validator,
    std::shared_ptr<ReplayProtectionEngine> replay_engine,
    std::shared_ptr<IMlAnomalyDetector> ml_detector,
    std::shared_ptr<GatewayLogger> logger,
    std::shared_ptr<IEthernetForwarder> forwarder)
    : secret_bytes_(secret_bytes)
    , validator_(std::move(validator))
    , replay_engine_(std::move(replay_engine))
    , ml_detector_(std::move(ml_detector))
    , logger_(std::move(logger))
    , forwarder_(std::move(forwarder))
{
    shared_secret_ = HmacSha256::bytesToHex(secret_bytes_.data(), secret_bytes_.size());
}

PipelineDecision GatewayPipeline::processFrame(const SecureCanMessage& msg) {
    auto start_time = std::chrono::steady_clock::now();
    uint64_t now_ms = getCurrentTimestampMillis();
    std::string now_iso = formatIsoTimestamp(now_ms);

    total_frames_++;
    PipelineDecision decision;
    decision.accepted = false;

    SecurityLogEntry log_entry;
    log_entry.timestamp_iso = now_iso;
    log_entry.timestamp_epoch_ms = now_ms;
    log_entry.can_id = msg.can_id;
    log_entry.sequence_number = msg.sequence_number;

    auto emitSecurityEvent = [this](const SecurityLogEntry& entry) {
        if (logger_) {
            logger_->log(entry);
        }
        if (forwarder_) {
            forwarder_->forwardSecurityAlert(entry.toJson());
        }
    };

    // ========================================================================
    // STAGE 1: CAN FRAME VALIDATION (Syntactic, Range & Semantic Physical Rules)
    // ========================================================================
    auto val_result = validator_->validateMessage(msg);
    if (!val_result.is_valid) {
        validation_failures_++;
        rejected_frames_++;

        decision.result = DecisionResult::REJECT_FRAME_VALIDATION_FAILED;
        decision.stage_failed = "FRAME_VALIDATION";
        decision.reason = val_result.failure_reason;

        // Classify attack type based on validation violation
        if (val_result.failure_reason.find("Unauthorized CAN ID") != std::string::npos) {
            decision.inferred_attack = AttackType::INJECTION_SPOOFING;
        } else if (val_result.failure_reason.find("Payload Length") != std::string::npos) {
            decision.inferred_attack = AttackType::FUZZY_TRAFFIC;
        } else {
            decision.inferred_attack = AttackType::FALSE_DATA_MANIPULATION;
        }

        log_entry.event_type = "VALIDATION_FAILED";
        log_entry.message_status = "REJECTED_DROPPED";
        log_entry.attack_type = decision.inferred_attack;
        log_entry.severity = Severity::CRITICAL;
        log_entry.details = val_result.failure_reason;

        emitSecurityEvent(log_entry);
        return decision;
    }

    // ========================================================================
    // STAGE 2: CRYPTOGRAPHIC HMAC-SHA256 AUTHENTICATION & INTEGRITY
    // ========================================================================
    // Notice: HMAC provides Authentication and Integrity, NOT Confidentiality.
    auto canonical = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
    std::string expected_hmac = HmacSha256::computeHex(secret_bytes_, canonical);

    bool hmac_ok = HmacSha256::verifyConstantTime(expected_hmac, msg.hmac_tag);
    if (!hmac_ok) {
        hmac_failures_++;

        if (permissive_mode_) {
            // Permissive Mode: Log warning alert for monitoring, but allow telemetry forwarding
            log_entry.event_type = "HMAC_AUTH_FAILED";
            log_entry.message_status = "FLAGGED_MONITORED";
            log_entry.hmac_result = "INVALID";
            log_entry.attack_type = AttackType::INJECTION_SPOOFING;
            log_entry.severity = Severity::WARNING;
            log_entry.details = "Permissive Mode: Unauthenticated frame allowed for telemetry monitoring";
            emitSecurityEvent(log_entry);
        } else {
            // Strict Zero-Trust Mode (Default): Drop immediately
            rejected_frames_++;
            decision.result = DecisionResult::REJECT_HMAC_AUTHENTICATION_FAILED;
            decision.stage_failed = "HMAC_VERIFICATION";
            decision.reason = "HMAC authentication mismatch or tag missing (Payload tampered or unauthorized sender)";
            decision.inferred_attack = AttackType::INJECTION_SPOOFING;

            log_entry.event_type = "HMAC_AUTH_FAILED";
            log_entry.message_status = "REJECTED_DROPPED";
            log_entry.hmac_result = "INVALID";
            log_entry.attack_type = AttackType::INJECTION_SPOOFING;
            log_entry.severity = Severity::CRITICAL;
            log_entry.details = decision.reason;

            emitSecurityEvent(log_entry);
            return decision;
        }
    } else {
        log_entry.hmac_result = "VALID";
    }

    // ========================================================================
    // STAGE 3: FRESHNESS & REPLAY COUNTER CHECK
    // ========================================================================
    auto replay_status = replay_engine_->validateSequence(msg.can_id, msg.sequence_number);
    if (!replay_status.is_valid) {
        replay_failures_++;

        if (permissive_mode_) {
            log_entry.event_type = "REPLAY_DETECTED";
            log_entry.message_status = "FLAGGED_MONITORED";
            log_entry.attack_type = AttackType::REPLAY_ATTACK;
            log_entry.severity = Severity::WARNING;
            log_entry.details = replay_status.reason;
            emitSecurityEvent(log_entry);
        } else {
            rejected_frames_++;
            decision.result = DecisionResult::REJECT_REPLAY_DETECTED;
            decision.stage_failed = "REPLAY_PROTECTION";
            decision.reason = replay_status.reason;
            decision.inferred_attack = AttackType::REPLAY_ATTACK;

            log_entry.event_type = "REPLAY_ATTACK_DETECTED";
            log_entry.message_status = "REJECTED_DROPPED";
            log_entry.attack_type = AttackType::REPLAY_ATTACK;
            log_entry.severity = Severity::CRITICAL;
            log_entry.details = replay_status.reason;

            emitSecurityEvent(log_entry);
            return decision;
        }
    }

    // ========================================================================
    // STAGE 4: MACHINE LEARNING BEHAVIORAL ANOMALY DETECTION
    // ========================================================================
    // ML is a secondary heuristic defense, NOT a replacement for cryptographic auth!
    if (ml_detector_) {
        VehicleFeatureVector features;
        auto* embedded_ml = dynamic_cast<EmbeddedMlDetector*>(ml_detector_.get());
        if (embedded_ml) {
            features = embedded_ml->extractFeatures(msg);
        } else {
            features.can_id = msg.can_id;
            features.speed_kmh = msg.telemetry.speed_kmh;
        }

        auto ml_result = ml_detector_->predict(features);
        decision.ml_anomaly_score = ml_result.anomaly_score;
        log_entry.ml_anomaly_score = ml_result.anomaly_score;

        if (ml_result.is_anomaly) {
            ml_anomalies_++;
            decision.stage_failed = "ML_ANOMALY_DETECTION";
            decision.reason = ml_result.explanation;

            if (ml_result.predicted_class == "DOS_FLOOD_ANOMALY") {
                decision.inferred_attack = AttackType::DOS_FLOODING;
            } else {
                decision.inferred_attack = AttackType::FALSE_DATA_MANIPULATION;
            }

            // Severe ML anomaly (e.g. DoS flood) causes rejection; moderate triggers alert & logging
            if (ml_result.anomaly_score >= 0.80) {
                rejected_frames_++;
                decision.result = DecisionResult::REJECT_ML_ANOMALY_DETECTED;

                log_entry.event_type = "ML_ANOMALY_REJECTED";
                log_entry.message_status = "REJECTED_DROPPED";
                log_entry.ml_result = ml_result.predicted_class;
                log_entry.attack_type = decision.inferred_attack;
                log_entry.severity = Severity::WARNING;
                log_entry.details = ml_result.explanation;

                emitSecurityEvent(log_entry);
                return decision;
            } else {
                // Flagged anomaly warning, but allowed to proceed with alert log
                log_entry.event_type = "ML_ANOMALY_FLAGGED";
                log_entry.message_status = "FLAGGED_MONITORED";
                log_entry.ml_result = ml_result.predicted_class;
                log_entry.severity = Severity::WARNING;
                log_entry.details = ml_result.explanation;
            }
        } else {
            log_entry.ml_result = "NORMAL";
        }
    } else {
        log_entry.ml_result = "DISABLED";
    }

    // ========================================================================
    // STAGE 5: FINAL DECISION & ARBITRATION
    // ========================================================================
    accepted_frames_++;
    decision.accepted = true;
    decision.result = DecisionResult::VALID_FORWARD;
    decision.stage_failed = "NONE";
    decision.reason = "Passed all validation, HMAC, freshness, and ML checks";
    decision.inferred_attack = AttackType::NONE;

    auto end_time = std::chrono::steady_clock::now();
    decision.processing_latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    // ========================================================================
    // STAGE 6: ETHERNET FORWARDING & LOGGING
    // ========================================================================
    if (forwarder_) {
        forwarder_->forwardTelemetry(msg);
    }

    if (log_entry.message_status != "FLAGGED_MONITORED") {
        log_entry.event_type = "FRAME_AUTHENTICATED";
        log_entry.message_status = "ACCEPTED_VALID";
        log_entry.attack_type = AttackType::NONE;
        log_entry.severity = Severity::INFO;
        std::ostringstream oss;
        oss << "Latency: " << decision.processing_latency_us << " us";
        log_entry.details = oss.str();
    }

    emitSecurityEvent(log_entry);

    return decision;
}

} // namespace autoshieldx
