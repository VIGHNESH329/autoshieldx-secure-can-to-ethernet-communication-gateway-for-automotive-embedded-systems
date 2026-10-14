#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * ============================================================================
 * CONTROLLED LABORATORY ATTACK SIMULATOR: ATTACK 5 (FALSE DATA / SIGNAL MANIPULATION)
 * ============================================================================
 * FOR LOCAL LAB SIMULATION ONLY.
 * NEVER USE AGAINST REAL VEHICLES OR PRODUCTION NETWORKS.
 *
 * Description:
 * In this attack scenario, sensor data is intentionally falsified or tampered with.
 * Even if an adversary possesses the cryptographic key (or compromises an ECU firmware),
 * injecting physically impossible sensor measurements can cause critical actuator
 * malfunction or trigger dangerous automated emergency braking.
 *
 * Attack Vectors Tested:
 * 1. Out-of-bounds speed: 320.0 km/h (> 250.0 km/h max threshold)
 * 2. Impossible RPM: 11,500 RPM (> 8,000 RPM redline limit)
 * 3. Severe thermal anomaly: 195.0 °C (> 125.0 °C maximum threshold)
 * 4. Cross-sensor physical inconsistency: Vehicle speed 160 km/h while
 *    wheel speed is 5 km/h (|Delta| = 155 km/h >> 35 km/h threshold).
 *
 * Expected Gateway Response:
 * - Stage 1 (Frame Validation): Enforces physical boundary and consistency rules,
 *   rejects manipulated frames, and logs FALSE_DATA_MANIPULATION alerts.
 * ============================================================================
 */

int main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Attack 5: False-Data Manipulation Simulator  \n";
    std::cout << "  TARGET: Local Simulated Gateway (127.0.0.1:15000)          \n";
    std::cout << "  WARNING: Local Laboratory Simulation Only                 \n";
    std::cout << "============================================================\n";

    std::string secret = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::string gateway_ip = "127.0.0.1";
    int gateway_port = 15000;
    int sender_port = 15015;
    std::string can_mode = "simulation";
    std::string can_iface = "can0";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--socketcan" || arg == "can0" || arg == "--can0") {
            can_mode = "socketcan";
            if (arg != "--socketcan" && arg != "--can0") can_iface = arg;
        } else if (arg == "--simulation") {
            can_mode = "simulation";
        } else if (arg.rfind("--iface=", 0) == 0) {
            can_iface = arg.substr(8);
        } else if (arg == "--iface" && i + 1 < argc) {
            can_iface = argv[++i];
        } else if (arg.rfind("--secret=", 0) == 0) {
            secret = arg.substr(9);
        } else if (arg == "--secret" && i + 1 < argc) {
            secret = argv[++i];
        } else if (std::isdigit(static_cast<unsigned char>(arg[0])) && i == 1) {
            gateway_port = std::stoi(arg);
        }
    }

    std::vector<uint8_t> secret_bytes = autoshieldx::HmacSha256::parseKey(secret);

    std::shared_ptr<autoshieldx::ICanChannel> channel;
    if (can_mode == "socketcan") {
        channel = std::make_shared<autoshieldx::SocketCanChannel>(can_iface);
    } else {
        channel = std::make_shared<autoshieldx::SimulatedCanChannel>("127.0.0.1", sender_port, gateway_ip, gateway_port);
    }

    if (!channel->init()) {
        std::cerr << "[Attack 5] Error: Failed to open CAN channel (" << channel->getChannelType() << ").\n";
        return 1;
    }

    std::cout << "[Attack 5] Channel active (" << channel->getChannelType() << "). Injecting manipulated sensor signals...\n\n";

    // Test 1: Out-of-bounds Speed (320 km/h)
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x100;
        msg.sequence_number = 801;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 320.0; // Out-of-bounds!
        msg.telemetry.rpm = 4000.0;
        msg.telemetry.temperature_c = 90.0;
        msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(320.0, 4000.0, 80.0, 90.0, 801);
        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon); // Even with authentic HMAC!

        std::cout << "[False Data 1] Speed = 320.0 km/h (Valid limit: 250 km/h) with signed HMAC...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Test 2: Impossible RPM (11,500 RPM)
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x100;
        msg.sequence_number = 802;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 120.0;
        msg.telemetry.rpm = 11500.0; // Impossible engine RPM!
        msg.telemetry.temperature_c = 92.0;
        msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(120.0, 11500.0, 90.0, 92.0, 802);
        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon);

        std::cout << "[False Data 2] RPM = 11,500 (Valid limit: 8,000 RPM) with signed HMAC...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Test 3: Severe Thermal Overheat (195 °C)
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x100;
        msg.sequence_number = 803;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 80.0;
        msg.telemetry.rpm = 3000.0;
        msg.telemetry.temperature_c = 195.0; // Out-of-bounds temperature!
        msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(80.0, 3000.0, 40.0, 195.0, 803);
        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon);

        std::cout << "[False Data 3] Temp = 195.0 C (Valid limit: 125 C) with signed HMAC...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Test 4: Physical Inconsistency between Vehicle Speed and Wheel Speed
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x200; // Dynamics
        msg.sequence_number = 804;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 160.0;       // Speedometer says 160 km/h
        msg.telemetry.wheel_speed_kmh = 5.0;   // Wheel sensor says 5 km/h (Delta = 155 km/h)
        msg.telemetry.steering_deg = 0.0;
        msg.telemetry.vibration_g = 0.2;
        msg.payload = autoshieldx::CanPayloadCodec::encodeDynamics(0.0, 5.0, 0.2, 804);
        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(msg.can_id, msg.sequence_number, msg.payload);
        msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon);

        std::cout << "[False Data 4] Physical Discrepancy: Vehicle Speed 160 km/h vs Wheel Speed 5 km/h...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[Attack 5] False-data manipulation simulation completed.\n";
    std::cout << "Verify Gateway Console/Logs: All physically impossible/manipulated frames must be REJECTED.\n";
    channel->close();
    return 0;
}
