#!/usr/bin/env python3
"""
AutoShieldX - Machine Learning Inference Engine
Provides a standalone Python prediction interface for anomaly detection.

CRITICAL ARCHITECTURE PRINCIPLE:
ML anomaly detection is an adaptive behavioral defense layer.
It complements, but NEVER replaces, cryptographic HMAC authentication and sequence freshness.
"""

import json
import os
import sys

class MlDetector:
    def __init__(self, config_path="ml/inference/model_weights.json"):
        self.config_path = config_path
        self.anomaly_threshold = 0.65
        self.dos_freq_threshold = 80.0
        self.max_seq_jump = 50.0
        self.vibration_spike = 2.5
        self.load_model()

    def load_model(self):
        if os.path.exists(self.config_path):
            with open(self.config_path, "r") as f:
                data = json.load(f)
                self.anomaly_threshold = data.get("anomaly_threshold", 0.65)
                self.dos_freq_threshold = data.get("dos_frequency_threshold_hz", 80.0)
                self.max_seq_jump = data.get("max_sequence_jump_threshold", 50.0)
                self.vibration_spike = data.get("vibration_spike_threshold_g", 2.5)
                print(f"[MlDetector] Loaded model configuration from {self.config_path}")
        else:
            print("[MlDetector] Using default anomaly boundary thresholds.")

    def predict(self, features):
        """
        Features input can be dict or list:
        [freq, can_id, speed, rpm, temp, vib, steer, wheel_speed, seq_delta, time_delta]
        """
        if isinstance(features, dict):
            freq = features.get("message_frequency_hz", 20.0)
            can_id = features.get("can_id", 256)
            speed = features.get("speed_kmh", 0.0)
            rpm = features.get("rpm", 850.0)
            temp = features.get("temperature_c", 85.0)
            vib = features.get("vibration_g", 0.1)
            steer = features.get("steering_deg", 0.0)
            wheel = features.get("wheel_speed_kmh", 0.0)
            seq_delta = features.get("sequence_delta", 1.0)
            time_delta = features.get("time_delta_ms", 50.0)
        else:
            freq, can_id, speed, rpm, temp, vib, steer, wheel, seq_delta, time_delta = features

        anomaly_score = 0.05
        is_anomaly = False
        predicted_class = "NORMAL"
        explanation = "Traffic characteristics match authentic driving profiles."

        # 1. DoS Flooding check
        if freq > self.dos_freq_threshold:
            anomaly_score = min(1.0, (freq - self.dos_freq_threshold) / 50.0 + 0.5)
            is_anomaly = True
            predicted_class = "DOS_FLOOD_ANOMALY"
            explanation = f"Message frequency of {freq:.1f} Hz exceeds threshold ({self.dos_freq_threshold} Hz)."
            return {
                "is_anomaly": is_anomaly,
                "anomaly_score": round(anomaly_score, 3),
                "predicted_class": predicted_class,
                "explanation": explanation
            }

        # 2. Sequence Discontinuity check
        if seq_delta > self.max_seq_jump or seq_delta <= 0:
            anomaly_score = 0.85
            is_anomaly = True
            predicted_class = "SEQUENCE_DISCONTINUITY"
            explanation = f"Abnormal sequence delta of {seq_delta} indicates potential replay or dropped stream."
            return {
                "is_anomaly": is_anomaly,
                "anomaly_score": round(anomaly_score, 3),
                "predicted_class": predicted_class,
                "explanation": explanation
            }

        # 3. Kinematic Outlier check
        if vib > self.vibration_spike:
            anomaly_score = 0.72
            is_anomaly = True
            predicted_class = "VIBRATION_SPIKE"
            explanation = f"Vibration reading of {vib:.2f} G exceeds safe vehicle dynamic limits."
            return {
                "is_anomaly": is_anomaly,
                "anomaly_score": round(anomaly_score, 3),
                "predicted_class": predicted_class,
                "explanation": explanation
            }

        if speed > 220.0 and temp < 35.0:
            anomaly_score = 0.70
            is_anomaly = True
            predicted_class = "CROSS_FEATURE_ANOMALY"
            explanation = "High speed with cold engine is a thermodynamically inconsistent state."
            return {
                "is_anomaly": is_anomaly,
                "anomaly_score": round(anomaly_score, 3),
                "predicted_class": predicted_class,
                "explanation": explanation
            }

        return {
            "is_anomaly": False,
            "anomaly_score": round(anomaly_score, 3),
            "predicted_class": "NORMAL",
            "explanation": explanation
        }

def main():
    detector = MlDetector()
    print("\n--- Running Sample ML Inference Tests ---")

    samples = [
        {"name": "Normal Cruising", "features": {"message_frequency_hz": 20.0, "can_id": 256, "speed_kmh": 65.0, "rpm": 2200.0, "temperature_c": 89.0, "vibration_g": 0.12, "steering_deg": 2.0, "wheel_speed_kmh": 65.1, "sequence_delta": 1.0, "time_delta_ms": 50.0}},
        {"name": "DoS Flooding Attack", "features": {"message_frequency_hz": 320.0, "can_id": 256, "speed_kmh": 70.0, "rpm": 2300.0, "temperature_c": 90.0, "vibration_g": 0.15, "steering_deg": 0.0, "wheel_speed_kmh": 70.0, "sequence_delta": 1.0, "time_delta_ms": 3.1}},
        {"name": "Replay Sequence Glitch", "features": {"message_frequency_hz": 20.0, "can_id": 256, "speed_kmh": 75.0, "rpm": 2400.0, "temperature_c": 90.0, "vibration_g": 0.15, "steering_deg": 0.0, "wheel_speed_kmh": 75.0, "sequence_delta": -15.0, "time_delta_ms": 50.0}},
        {"name": "Shock/Vibration Spike", "features": {"message_frequency_hz": 20.0, "can_id": 256, "speed_kmh": 50.0, "rpm": 1800.0, "temperature_c": 85.0, "vibration_g": 3.8, "steering_deg": 0.0, "wheel_speed_kmh": 50.0, "sequence_delta": 1.0, "time_delta_ms": 50.0}}
    ]

    for sample in samples:
        res = detector.predict(sample["features"])
        status = "ANOMALY DETECTED" if res["is_anomaly"] else "NORMAL"
        print(f"\nTest: {sample['name']}")
        print(f"  Classification : {res['predicted_class']} [{status}]")
        print(f"  Anomaly Score  : {res['anomaly_score']}")
        print(f"  Explanation    : {res['explanation']}")

if __name__ == "__main__":
    main()
