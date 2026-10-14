#pragma once

#include "autoshieldx/can/can_frame.hpp"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <deque>

namespace autoshieldx {

/**
 * @brief Extracted Network and Vehicle Feature Vector for ML Inference
 */
struct VehicleFeatureVector {
    double message_frequency_hz{0.0};
    double can_id{0.0};
    double speed_kmh{0.0};
    double rpm{0.0};
    double temperature_c{0.0};
    double vibration_g{0.0};
    double steering_deg{0.0};
    double wheel_speed_kmh{0.0};
    double sequence_delta{0.0};
    double time_delta_ms{0.0};

    std::vector<double> toVector() const {
        return {
            message_frequency_hz,
            can_id,
            speed_kmh,
            rpm,
            temperature_c,
            vibration_g,
            steering_deg,
            wheel_speed_kmh,
            sequence_delta,
            time_delta_ms
        };
    }
};

struct MlInferenceResult {
    bool is_anomaly{false};
    double anomaly_score{0.0}; // [0.0 = normal, 1.0 = severe anomaly]
    std::string predicted_class{"NORMAL"};
    std::string explanation{};
};

/**
 * @brief Abstract Machine Learning Anomaly Detector Interface
 *
 * Keeps ML modular and replaceable so users can plug in any
 * model trained in Python (Isolation Forest, One-Class SVM, Random Forest).
 */
class IMlAnomalyDetector {
public:
    virtual ~IMlAnomalyDetector() = default;

    virtual bool init(const std::string& model_config_path) = 0;
    virtual MlInferenceResult predict(const VehicleFeatureVector& features) = 0;
    virtual std::string getModelName() const = 0;
};

/**
 * @brief Embedded Real-Time ML Anomaly Detector
 *
 * Computes sliding-window message frequencies, sequence continuity heuristics,
 * and multi-variate statistical/decision-tree boundary rules exported
 * from the Python training pipeline.
 *
 * Runs deterministically in under 5 microseconds per frame on embedded Linux / Pi 4.
 */
class EmbeddedMlDetector : public IMlAnomalyDetector {
public:
    explicit EmbeddedMlDetector(double anomaly_threshold = 0.65, double dos_freq_threshold_hz = 80.0);
    ~EmbeddedMlDetector() override = default;

    bool init(const std::string& model_config_path) override;
    MlInferenceResult predict(const VehicleFeatureVector& features) override;
    std::string getModelName() const override { return "Embedded_Trained_Rule_Tree_Detector"; }

    // Feature extraction from streaming message context
    VehicleFeatureVector extractFeatures(const SecureCanMessage& msg);

    void setAnomalyThreshold(double threshold) { anomaly_threshold_ = threshold; }
    void setDosFrequencyThreshold(double hz) { dos_freq_threshold_hz_ = hz; }

private:
    double anomaly_threshold_;
    double dos_freq_threshold_hz_;

    // Sliding window for frequency calculation per CAN ID
    std::unordered_map<uint32_t, std::deque<std::chrono::steady_clock::time_point>> arrival_history_;
    std::unordered_map<uint32_t, uint32_t> last_seen_sequence_;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> last_seen_time_;
};

} // namespace autoshieldx
