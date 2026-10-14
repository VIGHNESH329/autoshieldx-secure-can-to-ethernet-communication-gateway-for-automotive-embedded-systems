#include "autoshieldx/gateway/security_pipeline.hpp"
#include "autoshieldx/validation/validation_engine.hpp"
#include "autoshieldx/security/replay_protection.hpp"
#include "autoshieldx/ml/ml_detector.hpp"
#include "autoshieldx/gateway/gateway_logger.hpp"
#include "autoshieldx/networking/ethernet_forwarder.hpp"
#include "autoshieldx/networking/aws_logger.hpp"
#include "autoshieldx/can/can_channel.hpp"
#include <iostream>
#include <fstream>
#include <csignal>
#include <atomic>
#include <thread>
#include <iomanip>

static std::atomic<bool> g_running{true};

void signalHandler(int /*signum*/) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Secure CAN-to-Ethernet Gateway (Simulation)  \n";
    std::cout << "  Target Architecture: Raspberry Pi 4 -> Ethernet -> Dashboard\n";
    std::cout << "  STATUS: Running in SOFTWARE SIMULATION MODE (No Hardware) \n";
    std::cout << "============================================================\n";

    std::string secret = "4175746f536869656c64585f5365637265745f4b65795f32303236214155544f";
    std::string listen_ip = "127.0.0.1";
    int listen_port = 15000;
    std::string ethernet_ip = "127.0.0.1";
    int ethernet_port = 16000;
    std::string can_mode = "simulation";
    std::string can_iface = "can0";
    uint32_t max_seq_gap = 50;
    bool log_console = true;

    // Dynamic resolution of paths so gateway functions whether executed from root, build/, or build/Release/
    std::string config_path = "config/gateway_config.json";
    std::string aws_config_path = "config/aws_config.json";
    std::string log_file_path = "logs/autoshieldx_security.log";
    std::string aws_log_path = "logs/aws_mock_telemetry_batch.json";

    auto fileExists = [](const std::string& p) {
        std::ifstream f(p);
        return f.good();
    };

    if (!fileExists(config_path) && fileExists(std::string("../") + config_path)) {
        config_path = std::string("../") + config_path;
        aws_config_path = std::string("../") + aws_config_path;
        log_file_path = std::string("../") + log_file_path;
        aws_log_path = std::string("../") + aws_log_path;
    } else if (!fileExists(config_path) && fileExists(std::string("../../") + config_path)) {
        config_path = std::string("../../") + config_path;
        aws_config_path = std::string("../../") + aws_config_path;
        log_file_path = std::string("../../") + log_file_path;
        aws_log_path = std::string("../../") + aws_log_path;
    }

    // Read full configuration file
    std::string cfg_str;
    std::ifstream cfg_in(config_path);
    if (cfg_in.is_open()) {
        std::stringstream buffer;
        buffer << cfg_in.rdbuf();
        cfg_str = buffer.str();
        auto extractJsonString = [](const std::string& s, const std::string& k, const std::string& d) {
            size_t pos = s.find("\"" + k + "\"");
            if (pos == std::string::npos) return d;
            size_t colon = s.find(':', pos);
            if (colon == std::string::npos) return d;
            size_t q1 = s.find('"', colon);
            if (q1 == std::string::npos || q1 > pos + 80) return d;
            size_t q2 = s.find('"', q1 + 1);
            if (q2 == std::string::npos) return d;
            return s.substr(q1 + 1, q2 - q1 - 1);
        };
        auto extractJsonInt = [](const std::string& s, const std::string& k, int d) {
            size_t pos = s.find("\"" + k + "\"");
            if (pos == std::string::npos) return d;
            size_t colon = s.find(':', pos);
            if (colon == std::string::npos) return d;
            size_t num_start = s.find_first_of("0123456789", colon);
            if (num_start != std::string::npos && num_start < colon + 15) {
                size_t num_end = s.find_first_not_of("0123456789", num_start);
                try { return std::stoi(s.substr(num_start, num_end - num_start)); } catch (...) {}
            }
            return d;
        };

        can_mode = extractJsonString(cfg_str, "mode", can_mode);
        can_iface = extractJsonString(cfg_str, "can_interface", can_iface);
        listen_port = extractJsonInt(cfg_str, "simulated_can_port", listen_port);
        ethernet_ip = extractJsonString(cfg_str, "ethernet_forward_ip", ethernet_ip);
        ethernet_port = extractJsonInt(cfg_str, "ethernet_forward_port", ethernet_port);
        secret = extractJsonString(cfg_str, "shared_secret_hex", secret);
        max_seq_gap = static_cast<uint32_t>(extractJsonInt(cfg_str, "max_allowed_sequence_gap", static_cast<int>(max_seq_gap)));
        log_file_path = extractJsonString(cfg_str, "log_file_path", log_file_path);
    }

    bool broadcast_enabled = false;
    bool permissive_mode = false;
    if (!cfg_str.empty()) {
        if (cfg_str.find("\"enable_broadcast\": true") != std::string::npos ||
            cfg_str.find("\"broadcast_enabled\": true") != std::string::npos ||
            cfg_str.find("\"ethernet_broadcast_enabled\": true") != std::string::npos) {
            broadcast_enabled = true;
        }
        if (cfg_str.find("\"permissive_mode\": true") != std::string::npos ||
            cfg_str.find("\"allow_unauthenticated\": true") != std::string::npos) {
            permissive_mode = true;
        }
    }


    // Check environment variables
    const char* env_mode = std::getenv("CAN_MODE");
    if (env_mode) {
        can_mode = env_mode;
    }
    const char* env_secret = std::getenv("AUTOSHIELDX_SECRET");
    if (env_secret) {
        secret = env_secret;
    }
    const char* env_eth_ip = std::getenv("AUTOSHIELDX_ETHERNET_IP");
    if (!env_eth_ip) env_eth_ip = std::getenv("ETHERNET_FORWARD_IP");
    if (!env_eth_ip) env_eth_ip = std::getenv("ETHERNET_IP");
    if (!env_eth_ip) env_eth_ip = std::getenv("DESTINATION_IP");
    if (env_eth_ip) {
        ethernet_ip = env_eth_ip;
    }
    const char* env_eth_port = std::getenv("AUTOSHIELDX_ETHERNET_PORT");
    if (!env_eth_port) env_eth_port = std::getenv("ETHERNET_PORT");
    if (env_eth_port) {
        try { ethernet_port = std::stoi(env_eth_port); } catch (...) {}
    }
    const char* env_bcast = std::getenv("AUTOSHIELDX_BROADCAST");
    if (env_bcast && (std::string(env_bcast) == "1" || std::string(env_bcast) == "true")) {
        broadcast_enabled = true;
    }

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            can_mode = argv[++i];
        } else if (arg == "--socketcan" || arg == "can0" || arg == "--can0") {
            can_mode = "socketcan";
            if (arg != "--socketcan" && arg != "--can0") can_iface = arg;
        } else if (arg == "--simulation") {
            can_mode = "simulation";
        } else if (arg.rfind("--iface=", 0) == 0) {
            can_iface = arg.substr(8);
        } else if (arg == "--iface" && i + 1 < argc) {
            can_iface = argv[++i];
        } else if (arg == "--secret" && i + 1 < argc) {
            secret = argv[++i];
        } else if ((arg == "--ethernet-ip" || arg == "--dest" || arg == "--destination" || arg == "--target" || arg == "--ip") && i + 1 < argc) {
            std::string dest_val = argv[++i];
            size_t colon = dest_val.find(':');
            if (colon != std::string::npos) {
                ethernet_ip = dest_val.substr(0, colon);
                try { ethernet_port = std::stoi(dest_val.substr(colon + 1)); } catch (...) {}
            } else {
                ethernet_ip = dest_val;
            }
        } else if (arg.rfind("--ethernet-ip=", 0) == 0) {
            std::string dest_val = arg.substr(14);
            size_t colon = dest_val.find(':');
            if (colon != std::string::npos) {
                ethernet_ip = dest_val.substr(0, colon);
                try { ethernet_port = std::stoi(dest_val.substr(colon + 1)); } catch (...) {}
            } else {
                ethernet_ip = dest_val;
            }
        } else if (arg.rfind("--dest=", 0) == 0) {
            std::string dest_val = arg.substr(7);
            size_t colon = dest_val.find(':');
            if (colon != std::string::npos) {
                ethernet_ip = dest_val.substr(0, colon);
                try { ethernet_port = std::stoi(dest_val.substr(colon + 1)); } catch (...) {}
            } else {
                ethernet_ip = dest_val;
            }
        } else if ((arg == "--ethernet-port" || arg == "--port") && i + 1 < argc) {
            ethernet_port = std::stoi(argv[++i]);
        } else if (arg == "--broadcast" || arg == "--lan-broadcast") {
            broadcast_enabled = true;
            if (ethernet_ip == "127.0.0.1") ethernet_ip = "255.255.255.255";
        } else if (arg.find('.') != std::string::npos) {
            // Positional argument with dots: IP address (e.g. 11.12.11.191 or 11.12.11.191:16000)
            size_t colon = arg.find(':');
            if (colon != std::string::npos) {
                ethernet_ip = arg.substr(0, colon);
                try { ethernet_port = std::stoi(arg.substr(colon + 1)); } catch (...) {}
            } else {
                ethernet_ip = arg;
            }
        } else if (arg == "--permissive" || arg == "--allow-unauthenticated" || arg == "--legacy") {
            permissive_mode = true;
        } else if (arg == "--strict") {
            permissive_mode = false;
        } else if (std::isdigit(static_cast<unsigned char>(arg[0]))) {
            if (i == 1) {
                listen_port = std::stoi(arg);
            } else if (i == 2) {
                ethernet_port = std::stoi(arg);
            }
        }
    }


    // Convert mode to lowercase for comparisons
    for (auto& c : can_mode) c = static_cast<char>(std::tolower(c));

    // Parse secret key (handles 64-char hex or raw string)
    std::vector<uint8_t> secret_bytes = autoshieldx::HmacSha256::parseKey(secret);

    std::cout << "============================================================\n";
    std::cout << "  AutoShieldX - Secure CAN-to-Ethernet Gateway              \n";
    std::cout << "  Target Architecture: Raspberry Pi 4 -> Ethernet -> Dashboard\n";
    if (can_mode == "socketcan") {
        std::cout << "  STATUS: Running in REAL HARDWARE MODE (SocketCAN / " << can_iface << ")\n";
    } else {
        std::cout << "  STATUS: Running in SOFTWARE SIMULATION MODE (UDP Loopback)\n";
    }
    std::cout << "  Crypto: RFC 2104 HMAC-SHA256 (Key Length: " << secret_bytes.size() << " bytes / " << (secret_bytes.size() * 8) << " bits)\n";
    std::cout << "============================================================\n";

    if (can_mode == "socketcan") {
        std::cout << "[Gateway] CAN Hardware Interface: " << can_iface << " (500 kbps Classic CAN)\n";
    } else {
        std::cout << "[Gateway] Listening for CAN frames on " << listen_ip << ":" << listen_port << "\n";
    }
    std::cout << "[Gateway] Forwarding validated telemetry to Ethernet " << ethernet_ip << ":" << ethernet_port << "\n";

    // 1. Initialize Validation Engine and load configured boundaries
    auto validator = std::make_shared<autoshieldx::ValidationEngine>();
    validator->loadConfig(config_path);

    // 2. Initialize Replay Protection Engine with configured gap
    auto replay_engine = std::make_shared<autoshieldx::ReplayProtectionEngine>(max_seq_gap);

    // 3. Initialize ML Behavioral Anomaly Detector
    auto ml_detector = std::make_shared<autoshieldx::EmbeddedMlDetector>(0.65, 80.0);
    ml_detector->init(config_path);

    // 4. Initialize Structured Security Logger and AWS Mock Persistence
    auto logger = std::make_shared<autoshieldx::GatewayLogger>(log_file_path, log_console);
    if (!logger->init()) {
        std::cerr << "[Gateway] Warning: Failed to open local security log file at " << log_file_path << ".\n";
    }

    auto aws_logger = std::make_shared<autoshieldx::LocalMockAwsLogger>(aws_log_path);
    aws_logger->init(aws_config_path);
    logger->setAwsLogger(aws_logger);

    // 5. Initialize Ethernet Forwarder
    auto forwarder = std::make_shared<autoshieldx::EthernetForwarder>(ethernet_ip, ethernet_port, broadcast_enabled);
    if (!forwarder->init()) {
        std::cerr << "[Gateway] Error: Failed to initialize Ethernet forwarder.\n";
    }


    // 6. Assemble Gateway Security Pipeline with exact decoded key bytes
    autoshieldx::GatewayPipeline pipeline(
        secret_bytes, validator, replay_engine, ml_detector, logger, forwarder);
    pipeline.setPermissiveMode(permissive_mode);

    if (permissive_mode) {
        std::cout << "[Gateway] SECURITY MODE: PERMISSIVE (Telemetry forwarded with warning if unauthenticated)\n";
    } else {
        std::cout << "[Gateway] SECURITY MODE: STRICT ZERO-TRUST (HMAC-SHA256 required, unauthenticated dropped)\n";
    }

    // 7. Initialize Inbound CAN Channel (Hardware SocketCAN or Simulation)
    std::shared_ptr<autoshieldx::ICanChannel> can_channel;
    if (can_mode == "socketcan") {
        can_channel = std::make_shared<autoshieldx::SocketCanChannel>(can_iface);
    } else {
        can_channel = std::make_shared<autoshieldx::SimulatedCanChannel>(listen_ip, listen_port);
    }

    if (!can_channel->init()) {
        std::cerr << "[Gateway] Error: Failed to initialize CAN channel (" << can_channel->getChannelType() << ").\n";
        if (can_mode == "socketcan") {
            std::cerr << "[Gateway] Diagnostic: Ensure can0 interface exists and is up ('sudo ip link set " 
                      << can_iface << " up').\n";
        }
        return 1;
    }

    std::cout << "[Gateway] Defense-in-depth pipeline armed and active (" << can_channel->getChannelType() << ").\n";
    std::cout << "Waiting for CAN messages (ECU Transmitter or Attack Injectors)...\n";
    std::cout << "Press Ctrl+C to terminate.\n\n";

    autoshieldx::SecureCanMessage msg;
    while (g_running) {
        if (can_channel->receive(msg, 200)) {
            pipeline.processFrame(msg);
        }
    }

    std::cout << "\n============================================================\n";
    std::cout << "  AutoShieldX Gateway Shutdown Summary                       \n";
    std::cout << "============================================================\n";
    std::cout << "Channel Type             : " << can_channel->getChannelType() << "\n";
    std::cout << "Total Frames Processed   : " << pipeline.getTotalFrames() << "\n";
    std::cout << "Frames Accepted (Valid)  : " << pipeline.getAcceptedFrames() << "\n";
    std::cout << "Frames Rejected (Dropped): " << pipeline.getRejectedFrames() << "\n";
    std::cout << "  - HMAC Auth Failures   : " << pipeline.getHmacFailures() << "\n";
    std::cout << "  - Replay Attack Rejects: " << pipeline.getReplayFailures() << "\n";
    std::cout << "  - Range/Validation Bad : " << pipeline.getValidationFailures() << "\n";
    std::cout << "  - ML Anomalies Detected: " << pipeline.getMlAnomalyCount() << "\n";
    std::cout << "Forwarded to Ethernet    : " << forwarder->getForwardedCount() << "\n";
    std::cout << "Local Log Path           : " << log_file_path << "\n";
    std::cout << "AWS Mock Batch Path      : " << aws_log_path << "\n";
    std::cout << "============================================================\n";

    can_channel->close();
    forwarder->close();
    return 0;
}
