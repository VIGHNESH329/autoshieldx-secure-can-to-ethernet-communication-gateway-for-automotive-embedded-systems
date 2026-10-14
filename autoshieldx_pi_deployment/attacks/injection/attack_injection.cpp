#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * ============================================================================
 * CONTROLLED LABORATORY ATTACK SIMULATOR: ATTACK 1 (CAN INJECTION & SPOOFING)
 * ============================================================================
 * FOR LOCAL LAB SIMULATION ONLY.
 * NEVER USE AGAINST REAL VEHICLES OR PRODUCTION NETWORKS.
 *
 * Description:
 * Injects unauthorized CAN frames into the simulated CAN channel:
 * 1. Forged messages with fake/random HMAC tags.
 * 2. Spoofed messages claiming to be Powertrain ECU (0x100) with empty HMAC.
 * 3. Injected messages using unauthorized/rogue CAN IDs (0x666, 0x555).
 *
 * Expected Gateway Response:
 * - Stage 1 (Frame Validation): Drops unauthorized CAN IDs.
 * - Stage 2 (HMAC Authentication): Drops counterfeit/unauthenticated frames.
 * ============================================================================
 */

int main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Attack 1: CAN Injection / Spoofing Simulator \n";
    std::cout << "  TARGET: Local Simulated Gateway (127.0.0.1:15000)          \n";
    std::cout << "  WARNING: Local Laboratory Simulation Only                 \n";
    std::cout << "============================================================\n";

    std::string gateway_ip = "127.0.0.1";
    int gateway_port = 15000;
    int sender_port = 15011;
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
        std::cerr << "[Attack 1] Error: Failed to open CAN channel (" << channel->getChannelType() << ").\n";
        return 1;
    }

    std::cout << "[Attack 1] Channel active (" << channel->getChannelType() << "). Injecting spoofed packets...\n\n";

    // Scenario A: Injected frame with forged invalid HMAC
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x100; // Powertrain
        msg.sequence_number = 100;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 90.0;
        msg.telemetry.rpm = 3200.0;
        msg.telemetry.temperature_c = 85.0;
        msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(90.0, 3200.0, 45.0, 85.0, 100);
        msg.hmac_tag = "deadbeefcafebabe0102030405060708deadbeefcafebabe0102030405060708"; // Forged tag!

        std::cout << "[Injecting Scenario A] Powertrain (0x100) with FORGED HMAC...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Scenario B: Injected frame with completely missing HMAC
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x100;
        msg.sequence_number = 101;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 110.0;
        msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(110.0, 3800.0, 50.0, 88.0, 101);
        msg.hmac_tag = ""; // Missing HMAC!

        std::cout << "[Injecting Scenario B] Powertrain (0x100) with MISSING HMAC...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Scenario C: Rogue unauthorized CAN ID (0x666)
    {
        autoshieldx::SecureCanMessage msg;
        msg.can_id = 0x666; // Rogue unwhitelisted ID!
        msg.sequence_number = 1;
        msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        msg.telemetry.speed_kmh = 50.0;
        msg.hmac_tag = "randomtag1234567890abcdef1234567890abcdef1234567890abcdef12345678";

        std::cout << "[Injecting Scenario C] Rogue Unwhitelisted CAN ID (0x666)...\n";
        channel->send(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[Attack 1] Injection sequence completed successfully.\n";
    std::cout << "Verify Gateway Console/Logs: All 3 injected frames must be REJECTED.\n";
    channel->close();
    return 0;
}
