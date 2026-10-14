#include "autoshieldx/ml/ml_detector.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace autoshieldx {

EmbeddedMlDetector::EmbeddedMlDetector(double anomaly_threshold, double dos_freq_threshold_hz)
    : anomaly_threshold_(anomaly_threshold)
    , dos_freq_threshold_hz_(dos_freq_threshold_hz)
{
}

bool EmbeddedMlDetector::init(const std::string& model_config_path) {
    if (model_config_path.empty()) {
        return true;
    }

    // Try loading model parameters/thresholds from JSON if present
    std::ifstream file(model_config_path);
    if (!file.is_open()) {
        std::cout << "[EmbeddedMlDetector] Using default pre-trained rule boundaries.\n";
        return true;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("\"anomaly_threshold\"") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                try {
                    anomaly_threshold_ = std::stod(line.substr(pos + 1));
                } catch (...) {}
            }
        } else if (line.find("\"dos_frequency_threshold_hz\"") != std::string::npos) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                try {
                    dos_freq_threshold_hz_ = std::stod(line.substr(pos + 1));
                } catch (...) {}
            }
        }
    }

    std::cout << "[EmbeddedMlDetector] Loaded model thresholds: Anomaly="
              << anomaly_threshold_ << ", DoS_Freq=" << dos_freq_threshold_hz_ << " Hz\n";
    return true;
}

VehicleFeatureVector EmbeddedMlDetector::extractFeatures(const SecureCanMessage& msg) {
    VehicleFeatureVector f;
    auto now = std::chrono::steady_clock::now();
    uint32_t cid = msg.can_id;

    // 1. Sliding window frequency calculation over last 1.0 second
    auto& history = arrival_history_[cid];
    history.push_back(now);

    auto cutoff = now - std::chrono::milliseconds(1000);
    while (!history.empty() && history.front() < cutoff) {
        history.pop_front();
    }
    f.message_frequency_hz = static_cast<double>(history.size());

    // 2. Telemetry and ID
    f.can_id = static_cast<double>(msg.can_id);
    f.speed_kmh = msg.telemetry.speed_kmh;
    f.rpm = msg.telemetry.rpm;
    f.temperature_c = msg.telemetry.temperature_c;
    f.vibration_g = msg.telemetry.vibration_g;
    f.steering_deg = msg.telemetry.steering_deg;
    f.wheel_speed_kmh = msg.telemetry.wheel_speed_kmh;

    // 3. Sequence delta and arrival timing
    if (last_seen_sequence_.find(cid) != last_seen_sequence_.end()) {
        f.sequence_delta = static_cast<double>(msg.sequence_number) - static_cast<double>(last_seen_sequence_[cid]);
    } else {
        f.sequence_delta = 1.0;
    }
    last_seen_sequence_[cid] = msg.sequence_number;

    if (last_seen_time_.find(cid) != last_seen_time_.end()) {
        auto diff_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_seen_time_[cid]).count();
        f.time_delta_ms = static_cast<double>(diff_us) / 1000.0;
    } else {
        f.time_delta_ms = 50.0;
    }
    last_seen_time_[cid] = now;

    return f;
}

MlInferenceResult EmbeddedMlDetector::predict(const VehicleFeatureVector& features) {
    MlInferenceResult res;
    res.is_anomaly = false;
    res.anomaly_score = 0.05; // Baseline normal
    res.predicted_class = "NORMAL";

    // 1. High-Frequency / Burst DoS Traffic Detection
    if (features.message_frequency_hz > dos_freq_threshold_hz_) {
        double severity = std::min(1.0, (features.message_frequency_hz - dos_freq_threshold_hz_) / 50.0 + 0.5);
        res.is_anomaly = true;
        res.anomaly_score = severity;
        res.predicted_class = "DOS_FLOOD_ANOMALY";
        std::ostringstream oss;
        oss << "Abnormal message frequency of " << features.message_frequency_hz
            << " Hz (threshold: " << dos_freq_threshold_hz_ << " Hz)";
        res.explanation = oss.str();
        return res;
    }

    // 2. Sequence Gap / Counter Glitch Anomaly
    if (features.sequence_delta > 50.0 || features.sequence_delta < 0.0) {
        res.is_anomaly = true;
        res.anomaly_score = 0.75;
        res.predicted_class = "SEQUENCE_DISCONTINUITY";
        std::ostringstream oss;
        oss << "Abrupt sequence delta of " << features.sequence_delta;
        res.explanation = oss.str();
        return res;
    }

    // 3. Dynamic Kinematic & Physical Plausibility Anomaly
    if (features.vibration_g > 3.0) {
        res.is_anomaly = true;
        res.anomaly_score = 0.70;
        res.predicted_class = "VIBRATION_SPIKE_ANOMALY";
        res.explanation = "Extreme shock/vibration signature detected on SW-420 model";
        return res;
    }

    if (features.speed_kmh > 200.0 && features.temperature_c < 30.0) {
        res.is_anomaly = true;
        res.anomaly_score = 0.68;
        res.predicted_class = "CROSS_FEATURE_INCONSISTENCY";
        res.explanation = "High speed with cold engine temperature represents an impossible thermodynamic state";
        return res;
    }

    res.is_anomaly = (res.anomaly_score >= anomaly_threshold_);
    res.explanation = "Features within standard driving bounds";
    return res;
}

} // namespace autoshieldx
