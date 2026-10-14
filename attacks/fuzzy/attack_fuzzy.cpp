#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

/**
 * ============================================================================
 * CONTROLLED LABORATORY ATTACK SIMULATOR: ATTACK 4 (FUZZY / RANDOM TRAFFIC)
 * ============================================================================
 * FOR LOCAL LAB SIMULATION ONLY.
 * NEVER USE AGAINST REAL VEHICLES OR PRODUCTION NETWORKS.
 *
 * Description:
 * Generates pseudorandomized, malformed, and fuzz-tested CAN messages:
 * - Randomized CAN IDs across the 11-bit identifier space [0x000 - 0x7FF]
 * - Corrupted payload bytes and malformed structures
 * - Arbitrary random HMAC authentication strings
 * Tests gateway parser robustness, memory safety, and boundary enforcement.
 *
 * Expected Gateway Response:
 * - Stage 1 (Frame Validation): Drops invalid CAN IDs, non-standard DLCs,
 *   and boundary violations.
 * - Stage 2 (HMAC Authentication): Drops unauthenticated garbage payloads.
 * ============================================================================
 */

int main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Attack 4: Fuzzy / Random Traffic Simulator   \n";
    std::cout << "  TARGET: Local Simulated Gateway (127.0.0.1:15000)          \n";
    std::cout << "  WARNING: Local Laboratory Simulation Only                 \n";
    std::cout << "============================================================\n";

    std::string gateway_ip = "127.0.0.1";
    int gateway_port = 15000;
    int sender_port = 15014;
    int count = 25;
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
        } else if (arg.rfind("--count=", 0) == 0) {
            count = std::stoi(arg.substr(8));
        } else if (std::isdigit(static_cast<unsigned char>(arg[0])) && i == 1) {
            gateway_port = std::stoi(arg);
        }
    }

    std::shared_ptr<autoshieldx::ICanChannel> channel;
    if (can_mode == "socketcan") {
        channel = std::make_shared<autoshieldx::SocketCanChannel>(can_iface);
    } else {
        channel = std::make_shared<autoshieldx::SimulatedCanChannel>("127.0.0.1", sender_port, gateway_ip, gateway_port);
    }

    if (!channel->init()) {
        std::cerr << "[Attack 4] Error: Failed to open CAN channel (" << channel->getChannelType() << ").\n";
        return 1;
    }

    std::mt19937 rng(1337);
    std::uniform_int_distribution<uint32_t> id_dist(0x001, 0x7FF);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_real_distribution<double> speed_dist(-50.0, 400.0);
    std::uniform_real_distribution<double> rpm_dist(-100.0, 15000.0);

    std::cout << "[Attack 4] Injecting " << count << " randomized fuzzy frames...\n";

    for (int i = 0; i < count; ++i) {
        autoshieldx::SecureCanMessage fuzzy_msg;
        fuzzy_msg.can_id = id_dist(rng);
        fuzzy_msg.sequence_number = static_cast<uint32_t>(i + 1);
        fuzzy_msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        fuzzy_msg.telemetry.speed_kmh = speed_dist(rng);
        fuzzy_msg.telemetry.rpm = rpm_dist(rng);
        fuzzy_msg.telemetry.temperature_c = static_cast<double>(byte_dist(rng));
        fuzzy_msg.telemetry.vibration_g = static_cast<double>(byte_dist(rng)) / 20.0;

        for (size_t b = 0; b < 8; ++b) {
            fuzzy_msg.payload[b] = static_cast<uint8_t>(byte_dist(rng));
        }

        // Random fake HMAC string
        std::ostringstream hmac_oss;
        for (int b = 0; b < 32; ++b) {
            hmac_oss << std::hex << (byte_dist(rng) % 16);
        }
        fuzzy_msg.hmac_tag = hmac_oss.str();

        channel->send(fuzzy_msg);
        std::cout << "  Fuzzed frame #" << (i + 1) << " (ID: 0x" << std::hex << fuzzy_msg.can_id << std::dec
                  << ", Speed: " << fuzzy_msg.telemetry.speed_kmh << " km/h)\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "\n[Attack 4] Fuzzing sequence completed.\n";
    std::cout << "Verify Gateway Console/Logs: All random/unauthorized frames must be cleanly rejected.\n";
    channel->close();
    return 0;
}
