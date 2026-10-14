#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include "autoshieldx/security/crypto_hmac.hpp"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * ============================================================================
 * CONTROLLED LABORATORY ATTACK SIMULATOR: ATTACK 2 (REPLAY ATTACK)
 * ============================================================================
 * FOR LOCAL LAB SIMULATION ONLY.
 * NEVER USE AGAINST REAL VEHICLES OR PRODUCTION NETWORKS.
 *
 * Description:
 * An adversary records an authentic signed CAN frame from the bus (e.g. sequence 10),
 * waits until legitimate communication has advanced to sequence 20+, and then
 * retransmits (replays) the recorded frame.
 *
 * Notice:
 * The HMAC of the replayed frame IS CRYPTOGRAPHICALLY VALID because the adversary
 * did not alter the payload or sequence number.
 * Therefore, HMAC alone DOES NOT PROTECT AGAINST REPLAY ATTACKS.
 * Protection is provided strictly by Stage 3 (Monotonic Sequence Counter Check).
 *
 * Expected Gateway Response:
 * - Stage 1 (Frame Validation): PASS
 * - Stage 2 (HMAC Authentication): PASS (authentic old signature)
 * - Stage 3 (Replay Check): REJECT (Incoming sequence <= Last accepted sequence)
 * ============================================================================
 */

int main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Attack 2: Replay Attack Simulator            \n";
    std::cout << "  TARGET: Local Simulated Gateway (127.0.0.1:15000)          \n";
    std::cout << "  WARNING: Local Laboratory Simulation Only                 \n";
    std::cout << "============================================================\n";

    std::string secret = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::string gateway_ip = "127.0.0.1";
    int gateway_port = 15000;
    int sender_port = 15012;
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
        std::cerr << "[Attack 2] Error: Failed to open CAN channel (" << channel->getChannelType() << ").\n";
        return 1;
    }

    // Step 1: Create an authentic message with sequence = 10 (Simulating captured frame)
    autoshieldx::SecureCanMessage captured_msg;
    captured_msg.can_id = 0x100;
    captured_msg.sequence_number = 10;
    captured_msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
    captured_msg.telemetry.speed_kmh = 75.0;
    captured_msg.telemetry.rpm = 2800.0;
    captured_msg.telemetry.throttle_pct = 30.0;
    captured_msg.telemetry.temperature_c = 85.0;
    captured_msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(75.0, 2800.0, 30.0, 85.0, 10);
    auto canon_captured = autoshieldx::HmacSha256::buildCanonicalBuffer(
        captured_msg.can_id, captured_msg.sequence_number, captured_msg.payload);
    captured_msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon_captured);

    std::cout << "[Step 1] Transmitting authentic captured message (Seq: 10, Speed: 75 km/h)...\n";
    channel->send(captured_msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Step 2: Transmit advancing authentic frames to move gateway's last accepted sequence to 15
    for (uint32_t s = 11; s <= 15; ++s) {
        autoshieldx::SecureCanMessage normal_msg;
        normal_msg.can_id = 0x100;
        normal_msg.sequence_number = s;
        normal_msg.timestamp_us = autoshieldx::getCurrentTimestampMicros();
        normal_msg.telemetry.speed_kmh = 75.0 + (s - 10) * 1.5;
        normal_msg.telemetry.rpm = 2900.0;
        normal_msg.telemetry.throttle_pct = 32.0;
        normal_msg.telemetry.temperature_c = 86.0;
        normal_msg.payload = autoshieldx::CanPayloadCodec::encodePowertrain(
            normal_msg.telemetry.speed_kmh, normal_msg.telemetry.rpm, normal_msg.telemetry.throttle_pct, normal_msg.telemetry.temperature_c, s);
        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(normal_msg.can_id, normal_msg.sequence_number, normal_msg.payload);
        normal_msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon);

        std::cout << "  [Advancing Traffic] Sent Seq: " << s << " (Speed: " << normal_msg.telemetry.speed_kmh << " km/h)\n";
        channel->send(normal_msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Step 3: Now launch the REPLAY ATTACK by re-injecting captured message (Seq: 10)
    std::cout << "\n[Step 3: LAUNCHING REPLAY ATTACK]\n";
    std::cout << "Retransmitting captured authentic message with STALE sequence 10 (Gateway last accepted is 15)...\n";
    channel->send(captured_msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Also replay a second time with another stale sequence (e.g. 12)
    {
        autoshieldx::SecureCanMessage stale_msg = captured_msg;
        stale_msg.sequence_number = 12;
        auto canon = autoshieldx::HmacSha256::buildCanonicalBuffer(stale_msg.can_id, stale_msg.sequence_number, stale_msg.payload);
        stale_msg.hmac_tag = autoshieldx::HmacSha256::computeHex(secret_bytes, canon);
        std::cout << "Retransmitting stale sequence 12 (12 <= 15)...\n";
        channel->send(stale_msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[Attack 2] Replay simulation finished.\n";
    std::cout << "Verify Gateway Console/Logs: Sequences <= 15 must be REJECTED as REPLAY_ATTACK.\n";
    channel->close();
    return 0;
}
