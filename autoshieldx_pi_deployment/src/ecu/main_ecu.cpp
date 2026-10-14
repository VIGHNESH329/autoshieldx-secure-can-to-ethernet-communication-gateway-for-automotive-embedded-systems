#include "autoshieldx/ecu/ecu_simulator.hpp"
#include "autoshieldx/ecu/sensor_interface.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include <iostream>
#include <fstream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> g_running{true};

void signalHandler(int /*signum*/) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Vehicle ECU Simulator (Software Simulation)  \n";
    std::cout << "  Target Architecture: Raspberry Pi Pico 2 -> MCP2515 CAN    \n";
    std::cout << "  STATUS: Running in SOFTWARE SIMULATION MODE (No Hardware) \n";
    std::cout << "============================================================\n";

    std::string secret = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::string gateway_ip = "127.0.0.1";
    int gateway_port = 15000;
    int local_port = 15001;

    // Load config if available
    std::string cfg_paths[] = {"config/gateway_config.json", "../config/gateway_config.json", "../../config/gateway_config.json"};
    for (const auto& p : cfg_paths) {
        std::ifstream f(p);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            auto getVal = [&](const std::string& key) -> std::string {
                size_t pos = content.find("\"" + key + "\"");
                if (pos == std::string::npos) return "";
                size_t colon = content.find(':', pos);
                if (colon == std::string::npos) return "";
                size_t q1 = content.find('"', colon);
                if (q1 != std::string::npos && q1 < pos + 60) {
                    size_t q2 = content.find('"', q1 + 1);
                    if (q2 != std::string::npos) return content.substr(q1 + 1, q2 - q1 - 1);
                }
                // Number check
                size_t num_start = content.find_first_of("0123456789", colon);
                if (num_start != std::string::npos && num_start < colon + 10) {
                    size_t num_end = content.find_first_not_of("0123456789", num_start);
                    return content.substr(num_start, num_end - num_start);
                }
                return "";
            };
            std::string s_hex = getVal("shared_secret_hex");
            if (!s_hex.empty()) secret = s_hex;
            std::string p_str = getVal("simulated_can_port");
            if (!p_str.empty()) {
                try { gateway_port = std::stoi(p_str); } catch (...) {}
            }
            break;
        }
    }

    if (argc >= 2) {
        gateway_ip = argv[1];
    }
    if (argc >= 3) {
        gateway_port = std::stoi(argv[2]);
    }

    std::cout << "[ECU Simulator] Target Gateway: " << gateway_ip << ":" << gateway_port << "\n";
    std::cout << "[ECU Simulator] Local Bind Port: " << local_port << "\n";

    // 1. Initialize Simulated Sensor Input (Kinematics & Dynamics)
    auto sensor_input = std::make_shared<autoshieldx::SimulatedSensorInput>();
    std::cout << "[ECU Simulator] Sensor Backend: " << sensor_input->getSourceDescription() << "\n";

    // 2. Initialize Simulated CAN Channel (UDP Loopback)
    auto can_channel = std::make_shared<autoshieldx::SimulatedCanChannel>(
        "127.0.0.1", local_port, gateway_ip, gateway_port);

    if (!can_channel->init()) {
        std::cerr << "[ECU Simulator] Error: Failed to initialize CAN channel.\n";
        return 1;
    }

    // 3. Initialize ECU Simulator with Cryptographic HMAC-SHA256
    auto ecu = std::make_unique<autoshieldx::EcuSimulator>(secret, sensor_input, can_channel);
    std::cout << "[ECU Simulator] HMAC-SHA256 Authenticator Initialized with Shared Secret.\n";
    std::cout << "[ECU Simulator] Transmitting authenticated telemetry at 20 Hz (every 50ms)...\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    uint64_t count = 0;
    while (g_running) {
        ecu->step(0.05); // Step by 50ms (20 Hz)
        count++;

        if (count % 20 == 0) {
            std::cout << "[ECU Telemetry #" << count << "] "
                      << "Speed: " << sensor_input->getSpeedKmh() << " km/h | "
                      << "RPM: " << sensor_input->getRpm() << " | "
                      << "Temp: " << sensor_input->getTemperatureC() << " C | "
                      << "Steer: " << sensor_input->getSteeringAngleDeg() << " deg | "
                      << "Vib: " << sensor_input->getVibrationG() << " G | "
                      << "Seq: " << ecu->getSequence(0x100) << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "\n[ECU Simulator] Shutting down. Sent " << count << " telemetry frames.\n";
    can_channel->close();
    return 0;
}
