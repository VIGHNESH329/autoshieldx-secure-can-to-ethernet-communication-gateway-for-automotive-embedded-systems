#include "autoshieldx/ecu/sensor_interface.hpp"
#include <algorithm>

namespace autoshieldx {

SimulatedSensorInput::SimulatedSensorInput()
    : speed_kmh_(0.0)
    , target_speed_kmh_(65.0)
    , rpm_(850.0)
    , temperature_c_(25.0)
    , vibration_g_(0.08)
    , steering_deg_(0.0)
    , throttle_pct_(0.0)
    , wheel_speed_kmh_(0.0)
    , elapsed_time_s_(0.0)
{
}

void SimulatedSensorInput::update(double dt_seconds) {
    elapsed_time_s_ += dt_seconds;

    // 1. Vehicle longitudinal acceleration dynamics
    double speed_error = target_speed_kmh_ - speed_kmh_;
    double accel_limit = 4.5; // km/h per second

    if (std::abs(speed_error) > 0.1) {
        double delta_v = std::clamp(speed_error * 0.4, -accel_limit, accel_limit) * dt_seconds;
        speed_kmh_ += delta_v;
    }
    speed_kmh_ = std::max(0.0, speed_kmh_);

    // 2. Throttle percentage correlated with acceleration demand
    if (speed_kmh_ < target_speed_kmh_) {
        throttle_pct_ = std::clamp(25.0 + (target_speed_kmh_ - speed_kmh_) * 1.5, 0.0, 85.0);
    } else {
        throttle_pct_ = std::clamp(12.0 + noise_dist_(rng_) * 2.0, 5.0, 30.0);
    }

    // 3. RPM simulation based on 5-speed transmission gear ratios
    double gear_ratio = 1.0;
    if (speed_kmh_ < 20.0) {
        gear_ratio = 80.0;  // 1st gear
    } else if (speed_kmh_ < 40.0) {
        gear_ratio = 50.0;  // 2nd gear
    } else if (speed_kmh_ < 65.0) {
        gear_ratio = 36.0;  // 3rd gear
    } else if (speed_kmh_ < 95.0) {
        gear_ratio = 26.0;  // 4th gear
    } else {
        gear_ratio = 20.0;  // 5th gear (Cruising)
    }

    double base_rpm = 850.0 + (speed_kmh_ * gear_ratio);
    rpm_ = std::clamp(base_rpm + noise_dist_(rng_) * 15.0, 800.0, 6500.0);

    // 4. Engine coolant thermal curve (warms to operating 90 C, then stabilizes)
    if (temperature_c_ < 88.0) {
        temperature_c_ += (0.15 * (1.0 + throttle_pct_ / 50.0)) * dt_seconds;
    } else {
        // Thermostat regulation oscillation around 90 C
        temperature_c_ = 90.0 + 1.5 * std::sin(elapsed_time_s_ * 0.05) + noise_dist_(rng_) * 0.1;
    }

    // 5. Road vibration simulation (simulating SW-420 sensor response)
    // Baseline vibration increases with speed + stochastic road texture
    double road_vib = 0.05 + (speed_kmh_ / 250.0) * 0.35 + std::abs(noise_dist_(rng_) * 0.08);
    vibration_g_ = std::clamp(road_vib, 0.02, 1.2);

    // Controlled, repeatable behavioral-anomaly test pulse (every 15 seconds)
    // Generates a transient physical vibration anomaly (3.4 G) that satisfies physical
    // range limits [0.0, 5.0 G] and HMAC validation, but triggers the ML anomaly detector (> 3.0 G).
    double cycle = std::fmod(elapsed_time_s_, 15.0);
    if (cycle >= 14.5 && cycle < 14.7) {
        vibration_g_ = 3.4;
    }

    // 6. Steering angle: gentle highway lane tracking sinusoid
    steering_deg_ = 4.0 * std::sin(elapsed_time_s_ * 0.2) + noise_dist_(rng_) * 0.5;

    // 7. Wheel speed tracks vehicle speed with minor tire slip dynamics
    wheel_speed_kmh_ = std::max(0.0, speed_kmh_ + noise_dist_(rng_) * 0.3);
}

} // namespace autoshieldx
