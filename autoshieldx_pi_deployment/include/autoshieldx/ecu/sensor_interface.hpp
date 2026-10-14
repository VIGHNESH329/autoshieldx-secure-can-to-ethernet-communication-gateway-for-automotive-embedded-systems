#pragma once

#include <string>
#include <memory>
#include <cmath>
#include <random>

namespace autoshieldx {

/**
 * @brief Abstract Sensor Input Interface
 *
 * Provides a clean hardware-agnostic boundary between vehicle telemetry acquisition
 * and the ECU simulator logic. This allows immediate testing with software physics models
 * and clean drop-in replacement with physical hardware sensors in Phase 2.
 */
class ISensorInput {
public:
    virtual ~ISensorInput() = default;

    virtual void update(double dt_seconds) = 0;

    virtual double getSpeedKmh() const = 0;
    virtual double getRpm() const = 0;
    virtual double getTemperatureC() const = 0;
    virtual double getVibrationG() const = 0;
    virtual double getSteeringAngleDeg() const = 0;
    virtual double getThrottlePercent() const = 0;
    virtual double getWheelSpeedKmh() const = 0;

    virtual std::string getSourceDescription() const = 0;
    virtual bool isPhysicalHardware() const = 0;
};

/**
 * @brief Software Simulation Sensor Input
 *
 * Generates realistic dynamic vehicle telemetry without physical sensors:
 * - Speed follows an acceleration/cruising/deceleration drive cycle.
 * - RPM correlates with speed and simulated 5-speed transmission gears.
 * - Engine temperature warms from ambient (25°C) to operating temp (90°C).
 * - Vibration simulates SW-420 baseline road vibration (0.05G - 0.35G) with road bump variations.
 * - Steering angle oscillates smoothly representing highway lane adjustments (-25° to +25°).
 * - Wheel speed tracks vehicle speed with minor tire slip dynamics.
 */
class SimulatedSensorInput : public ISensorInput {
public:
    SimulatedSensorInput();
    ~SimulatedSensorInput() override = default;

    void update(double dt_seconds) override;

    double getSpeedKmh() const override { return speed_kmh_; }
    double getRpm() const override { return rpm_; }
    double getTemperatureC() const override { return temperature_c_; }
    double getVibrationG() const override { return vibration_g_; }
    double getSteeringAngleDeg() const override { return steering_deg_; }
    double getThrottlePercent() const override { return throttle_pct_; }
    double getWheelSpeedKmh() const override { return wheel_speed_kmh_; }

    std::string getSourceDescription() const override { return "Software_Simulated_Vehicle_Kinematics"; }
    bool isPhysicalHardware() const override { return false; }

    // Simulation control helpers
    void setTargetSpeed(double target_kmh) { target_speed_kmh_ = target_kmh; }
    void injectAbnormalVibration(double g) { vibration_g_ = g; }
    void injectAbnormalTemperature(double c) { temperature_c_ = c; }

private:
    double speed_kmh_{0.0};
    double target_speed_kmh_{65.0};
    double rpm_{800.0};
    double temperature_c_{25.0};
    double vibration_g_{0.1};
    double steering_deg_{0.0};
    double throttle_pct_{15.0};
    double wheel_speed_kmh_{0.0};

    double elapsed_time_s_{0.0};
    std::mt19937 rng_{42};
    std::normal_distribution<double> noise_dist_{0.0, 0.05};
};

/**
 * @brief Blueprint for Future Real Sensor Input (Raspberry Pi Pico 2 / GPIO / ADC)
 *
 * IMPORTANT NOTE:
 * Physical sensors are NOT connected in Phase 1 software simulation.
 * This class documents the future interface implementation for Phase 2:
 * - Potentiometer: Connected to Pico 2 ADC0 (GPIO 26) -> maps 0-3.3V to 0-100% throttle.
 * - Temperature Sensor: DS18B20 1-Wire / thermistor on ADC1 (GPIO 27) -> engine temperature.
 * - SW-420 Vibration Sensor: Digital comparator / ADC2 (GPIO 28) -> vibration acceleration in G.
 * - Wheel Speed Sensor: Hall-effect sensor / optical encoder on GPIO 15 (Edge Interrupt).
 */
class RealSensorInput : public ISensorInput {
public:
    RealSensorInput() = default;
    ~RealSensorInput() override = default;

    void update(double /*dt_seconds*/) override {
        // Real hardware sampling stub - active only when deployed on physical Pico 2 SDK
    }

    double getSpeedKmh() const override { return 0.0; }
    double getRpm() const override { return 0.0; }
    double getTemperatureC() const override { return 25.0; }
    double getVibrationG() const override { return 0.0; }
    double getSteeringAngleDeg() const override { return 0.0; }
    double getThrottlePercent() const override { return 0.0; }
    double getWheelSpeedKmh() const override { return 0.0; }

    std::string getSourceDescription() const override {
        return "Phase2_Real_Hardware_Sensors (NOT_CONNECTED - Simulation Mode)";
    }
    bool isPhysicalHardware() const override { return true; }
};

} // namespace autoshieldx
