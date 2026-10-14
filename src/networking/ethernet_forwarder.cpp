#include "autoshieldx/networking/ethernet_forwarder.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  #define CLOSE_SOCKET(s) closesocket(s)
  #define IS_INVALID(s) ((s) == INVALID_SOCKET)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <cerrno>
  using socket_t = int;
  #define CLOSE_SOCKET(s) ::close(s)
  #define IS_INVALID(s) ((s) < 0)
#endif

namespace autoshieldx {

EthernetForwarder::EthernetForwarder(const std::string& target_ip, int target_port, bool enable_broadcast)
    : target_ip_(target_ip)
    , target_port_(target_port)
    , broadcast_enabled_(enable_broadcast)
{
}

EthernetForwarder::~EthernetForwarder() {
    close();
}

std::string EthernetForwarder::getSocketErrorString() {
#ifdef _WIN32
    int err = WSAGetLastError();
    char msg_buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   msg_buf, sizeof(msg_buf), NULL);
    std::string msg(msg_buf);
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n')) {
        msg.pop_back();
    }
    return "Winsock error " + std::to_string(err) + " (" + msg + ")";
#else
    return std::string(strerror(errno)) + " (errno " + std::to_string(errno) + ")";
#endif
}

bool EthernetForwarder::resolveAddresses() {
    std::memset(&dest_addr_, 0, sizeof(dest_addr_));
    dest_addr_.sin_family = AF_INET;
    dest_addr_.sin_port = htons(static_cast<uint16_t>(target_port_));

    std::memset(&broadcast_addr_, 0, sizeof(broadcast_addr_));
    broadcast_addr_.sin_family = AF_INET;
    broadcast_addr_.sin_port = htons(static_cast<uint16_t>(target_port_));
    broadcast_addr_.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    is_target_broadcast_ = false;

    if (target_ip_ == "255.255.255.255" || target_ip_ == "broadcast" || target_ip_ == "auto") {
        dest_addr_.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        is_target_broadcast_ = true;
        return true;
    }

    int pton_res = inet_pton(AF_INET, target_ip_.c_str(), &dest_addr_.sin_addr);
    if (pton_res > 0) {
        return true;
    }

    // Fallback: try hostname resolution (e.g. localhost or network node name)
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(target_ip_.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
        auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        dest_addr_.sin_addr = ipv4->sin_addr;
        freeaddrinfo(res);
        return true;
    }

    std::cerr << "[ETHERNET] Resolution error: Unable to parse IP address '" << target_ip_ << "'\n";
    return false;
}

bool EthernetForwarder::init() {
#ifdef _WIN32
    WSADATA wsa;
    int wsa_res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsa_res != 0) {
        std::cerr << "[ETHERNET] INIT FAILED: WSAStartup error " << wsa_res << "\n";
        return false;
    }
#endif

    socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (IS_INVALID(s)) {
        std::cerr << "[ETHERNET] INIT FAILED: Cannot create UDP socket: " << getSocketErrorString() << "\n";
        return false;
    }

    // 1. Enable SO_BROADCAST unconditionally so broadcast sends never fail with EACCES
    int broadcast_opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast_opt), sizeof(broadcast_opt)) != 0) {
        std::cerr << "[ETHERNET] Warning: setsockopt(SO_BROADCAST) returned error: " << getSocketErrorString() << "\n";
    }

    // 2. Enable SO_REUSEADDR for rapid port recycling
    int reuse_opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse_opt), sizeof(reuse_opt));

    socket_fd_ = static_cast<int>(s);
    initialized_ = true;

    if (!resolveAddresses()) {
        std::cerr << "[ETHERNET] INIT WARNING: Defaulted to 127.0.0.1 due to resolution failure.\n";
        target_ip_ = "127.0.0.1";
        dest_addr_.sin_family = AF_INET;
        dest_addr_.sin_port = htons(static_cast<uint16_t>(target_port_));
        inet_pton(AF_INET, "127.0.0.1", &dest_addr_.sin_addr);
    }

    // Required clear runtime logging
    std::cout << "[ETHERNET] Destination: " << getDestination() << std::endl;
    if (broadcast_enabled_ && !is_target_broadcast_) {
        std::cout << "[ETHERNET] LAN Broadcast mirror enabled: 255.255.255.255:" << target_port_ << std::endl;
    }

    return true;
}

void EthernetForwarder::setDestination(const std::string& target_ip, int target_port) {
    target_ip_ = target_ip;
    target_port_ = target_port;
    if (initialized_) {
        resolveAddresses();
        std::cout << "[ETHERNET] Destination updated: " << getDestination() << std::endl;
    }
}

void EthernetForwarder::setBroadcast(bool enable) {
    broadcast_enabled_ = enable;
    if (initialized_) {
        std::cout << "[ETHERNET] Broadcast mode " << (enable ? "enabled" : "disabled") << std::endl;
    }
}

void EthernetForwarder::close() {
    if (initialized_ && socket_fd_ >= 0) {
        CLOSE_SOCKET(static_cast<socket_t>(socket_fd_));
        socket_fd_ = -1;
        initialized_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

bool EthernetForwarder::sendRawString(const std::string& payload) {
    if (!initialized_ || socket_fd_ < 0) {
        std::cerr << "[ETHERNET] SEND FAILED error=Socket not initialized" << std::endl;
        return false;
    }

    int sent = sendto(
        static_cast<socket_t>(socket_fd_),
        payload.c_str(),
        static_cast<int>(payload.length()),
        0,
        reinterpret_cast<const sockaddr*>(&dest_addr_),
        sizeof(dest_addr_));

    if (sent > 0) {
        forwarded_count_++;
        std::cout << "[ETHERNET] JSON SENT bytes=" << sent << std::endl;
    } else {
        std::string err_str = getSocketErrorString();
        std::cerr << "[ETHERNET] SEND FAILED error=" << err_str << std::endl;
    }


    // Send copy across LAN broadcast if broadcast mirroring is enabled and target is not already broadcast
    if (broadcast_enabled_ && !is_target_broadcast_) {
        int bsent = sendto(
            static_cast<socket_t>(socket_fd_),
            payload.c_str(),
            static_cast<int>(payload.length()),
            0,
            reinterpret_cast<const sockaddr*>(&broadcast_addr_),
            sizeof(broadcast_addr_));
        if (bsent <= 0 && sent <= 0) {
            // Log if broadcast also encountered error
            std::cerr << "[ETHERNET] Broadcast send failed: " << getSocketErrorString() << "\n";
        }
    }

    return (sent > 0);
}

bool EthernetForwarder::forwardTelemetry(const SecureCanMessage& msg) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"VEHICLE_TELEMETRY\","
        << "\"can_id\":" << msg.can_id << ","
        << "\"sequence_number\":" << msg.sequence_number << ","
        << "\"timestamp_us\":" << msg.timestamp_us << ","
        << "\"speed_kmh\":" << std::fixed << std::setprecision(2) << msg.telemetry.speed_kmh << ","
        << "\"rpm\":" << msg.telemetry.rpm << ","
        << "\"temperature_c\":" << msg.telemetry.temperature_c << ","
        << "\"vibration_g\":" << std::setprecision(4) << msg.telemetry.vibration_g << ","
        << "\"steering_deg\":" << std::setprecision(2) << msg.telemetry.steering_deg << ","
        << "\"throttle_pct\":" << msg.telemetry.throttle_pct << ","
        << "\"wheel_speed_kmh\":" << msg.telemetry.wheel_speed_kmh << ","
        << "\"hmac_verified\":true,"
        << "\"gateway_device\":\"RaspberryPi4_SecureGateway\""
        << "}";

    return sendRawString(oss.str());
}

bool EthernetForwarder::forwardSecurityAlert(const std::string& alert_json) {
    return sendRawString(alert_json);
}

std::string EthernetForwarder::getDestination() const {
    return target_ip_ + ":" + std::to_string(target_port_);
}

} // namespace autoshieldx
