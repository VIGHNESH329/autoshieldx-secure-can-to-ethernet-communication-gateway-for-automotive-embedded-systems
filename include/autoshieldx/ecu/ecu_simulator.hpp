#pragma once

#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include "autoshieldx/ecu/sensor_interface.hpp"
#include <memory>
#include <string>
#include <atomic>
#include <thread>

namespace autoshieldx {

/**
 * @brief ECU Simulator Component
 *
 * Simulates a vehicle electronic control unit (such as Powertrain ECU / Body Control Module)
 * running on the future Raspberry Pi Pico 2.
 *
 * Operational flow:
 * 1. Read vehicle telemetry from ISensorInput (Simulated in Phase 1, ADC/GPIO in Phase 2)
 * 2. Encode parameters into standard 8-byte CAN raw payload
 * 3. Increment monotonic sequence counter
 * 4. Compute cryptographic HMAC-SHA256 authentication tag
 * 5. Transmit authenticated SecureCanMessage across CAN channel
 */
class EcuSimulator {
public:
    EcuSimulator(
        const std::string& shared_secret,
        std::shared_ptr<ISensorInput> sensor_input,
        std::shared_ptr<ICanChannel> can_channel);

    ~EcuSimulator();

    /**
     * @brief Generate next authenticated Powertrain telemetry frame (CAN ID 0x100).
     */
    SecureCanMessage buildPowertrainMessage();

    /**
     * @brief Generate next authenticated Dynamics telemetry frame (CAN ID 0x200).
     */
    SecureCanMessage buildDynamicsMessage();

    /**
     * @brief Step simulation by dt seconds and send messages.
     */
    bool step(double dt_seconds);

    /**
     * @brief Run continuous periodic transmission loop in background or current thread.
     */
    void startPeriodicTransmission(int interval_ms = 50);
    void stopPeriodicTransmission();

    // Sequence queries
    uint32_t getSequence(uint32_t can_id) const;

private:
    std::string shared_secret_;
    std::vector<uint8_t> secret_bytes_;
    std::shared_ptr<ISensorInput> sensor_input_;
    std::shared_ptr<ICanChannel> can_channel_;

    std::unordered_map<uint32_t, uint32_t> sequence_counters_;
    std::atomic<bool> is_running_{false};
    std::unique_ptr<std::thread> worker_thread_;
};

} // namespace autoshieldx
