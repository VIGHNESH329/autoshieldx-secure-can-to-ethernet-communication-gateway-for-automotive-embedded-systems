#include "autoshieldx/ecu/ecu_simulator.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include <iostream>
#include <chrono>

namespace autoshieldx {

EcuSimulator::EcuSimulator(
    const std::string& shared_secret,
    std::shared_ptr<ISensorInput> sensor_input,
    std::shared_ptr<ICanChannel> can_channel)
    : shared_secret_(shared_secret)
    , sensor_input_(std::move(sensor_input))
    , can_channel_(std::move(can_channel))
{
    secret_bytes_ = HmacSha256::parseKey(shared_secret_);
    sequence_counters_[static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY)] = 1;
    sequence_counters_[static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY)] = 1;
}

EcuSimulator::~EcuSimulator() {
    stopPeriodicTransmission();
}

SecureCanMessage EcuSimulator::buildPowertrainMessage() {
    uint32_t can_id = static_cast<uint32_t>(CanMessageId::POWERTRAIN_TELEMETRY);
    uint32_t seq = sequence_counters_[can_id]++;

    SecureCanMessage msg;
    msg.can_id = can_id;
    msg.sequence_number = seq;
    msg.timestamp_us = getCurrentTimestampMicros();

    // Populate telemetry snapshot
    msg.telemetry.speed_kmh = sensor_input_->getSpeedKmh();
    msg.telemetry.rpm = sensor_input_->getRpm();
    msg.telemetry.throttle_pct = sensor_input_->getThrottlePercent();
    msg.telemetry.temperature_c = sensor_input_->getTemperatureC();
    msg.telemetry.wheel_speed_kmh = sensor_input_->getWheelSpeedKmh();
    msg.telemetry.vibration_g = sensor_input_->getVibrationG();
    msg.telemetry.steering_deg = sensor_input_->getSteeringAngleDeg();

    // Encode into raw 8-byte CAN payload
    msg.payload = CanPayloadCodec::encodePowertrain(
        msg.telemetry.speed_kmh,
        msg.telemetry.rpm,
        msg.telemetry.throttle_pct,
        msg.telemetry.temperature_c,
        seq);

    // Cryptographic HMAC-SHA256 generation over canonical buffer
    auto canonical = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
    msg.hmac_tag = HmacSha256::computeHex(secret_bytes_, canonical);

    return msg;
}

SecureCanMessage EcuSimulator::buildDynamicsMessage() {
    uint32_t can_id = static_cast<uint32_t>(CanMessageId::DYNAMICS_TELEMETRY);
    uint32_t seq = sequence_counters_[can_id]++;

    SecureCanMessage msg;
    msg.can_id = can_id;
    msg.sequence_number = seq;
    msg.timestamp_us = getCurrentTimestampMicros();

    // Populate telemetry snapshot
    msg.telemetry.speed_kmh = sensor_input_->getSpeedKmh();
    msg.telemetry.rpm = sensor_input_->getRpm();
    msg.telemetry.throttle_pct = sensor_input_->getThrottlePercent();
    msg.telemetry.temperature_c = sensor_input_->getTemperatureC();
    msg.telemetry.steering_deg = sensor_input_->getSteeringAngleDeg();
    msg.telemetry.wheel_speed_kmh = sensor_input_->getWheelSpeedKmh();
    msg.telemetry.vibration_g = sensor_input_->getVibrationG();

    // Encode into raw 8-byte CAN payload
    msg.payload = CanPayloadCodec::encodeDynamics(
        msg.telemetry.steering_deg,
        msg.telemetry.wheel_speed_kmh,
        msg.telemetry.vibration_g,
        seq);

    // Cryptographic HMAC-SHA256 generation over canonical buffer
    auto canonical = HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
    msg.hmac_tag = HmacSha256::computeHex(secret_bytes_, canonical);

    return msg;
}

bool EcuSimulator::step(double dt_seconds) {
    if (sensor_input_) {
        sensor_input_->update(dt_seconds);
    }

    auto pt_msg = buildPowertrainMessage();
    bool sent1 = can_channel_->send(pt_msg);

    auto dyn_msg = buildDynamicsMessage();
    bool sent2 = can_channel_->send(dyn_msg);

    return (sent1 && sent2);
}

void EcuSimulator::startPeriodicTransmission(int interval_ms) {
    if (is_running_) return;

    is_running_ = true;
    worker_thread_ = std::make_unique<std::thread>([this, interval_ms]() {
        double dt = static_cast<double>(interval_ms) / 1000.0;
        while (is_running_) {
            step(dt);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    });
}

void EcuSimulator::stopPeriodicTransmission() {
    if (is_running_) {
        is_running_ = false;
        if (worker_thread_ && worker_thread_->joinable()) {
            worker_thread_->join();
        }
        worker_thread_.reset();
    }
}

uint32_t EcuSimulator::getSequence(uint32_t can_id) const {
    auto it = sequence_counters_.find(can_id);
    if (it != sequence_counters_.end()) {
        return it->second;
    }
    return 0;
}

} // namespace autoshieldx
