#include "autoshieldx/can/can_channel.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
  #define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
  #define CLOSE_SOCKET(s) closesocket(s)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  using socket_t = int;
  #define INVALID_SOCKET (-1)
  #define IS_INVALID_SOCKET(s) ((s) < 0)
  #define CLOSE_SOCKET(s) ::close(s)
#endif

namespace autoshieldx {

SimulatedCanChannel::SimulatedCanChannel(
    const std::string& bind_ip, int bind_port,
    const std::string& peer_ip, int peer_port)
    : bind_ip_(bind_ip)
    , bind_port_(bind_port)
    , peer_ip_(peer_ip)
    , peer_port_(peer_port)
{
}

SimulatedCanChannel::~SimulatedCanChannel() {
    close();
}

bool SimulatedCanChannel::init() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[SimulatedCanChannel] WSAStartup failed.\n";
        return false;
    }
#endif

    socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (IS_INVALID_SOCKET(s)) {
        std::cerr << "[SimulatedCanChannel] Failed to create UDP socket.\n";
        return false;
    }

    // Set reuse address
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Bind local receiving endpoint
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(static_cast<uint16_t>(bind_port_));
    inet_pton(AF_INET, bind_ip_.c_str(), &local_addr.sin_addr);

    if (bind(s, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) != 0) {
        std::cerr << "[SimulatedCanChannel] Failed to bind to " << bind_ip_ << ":" << bind_port_ << "\n";
        CLOSE_SOCKET(s);
        return false;
    }

    socket_fd_ = static_cast<int>(s);
    initialized_ = true;
    return true;
}

void SimulatedCanChannel::close() {
    if (initialized_ && socket_fd_ >= 0) {
        CLOSE_SOCKET(static_cast<socket_t>(socket_fd_));
        socket_fd_ = -1;
        initialized_ = false;
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

bool SimulatedCanChannel::send(const SecureCanMessage& msg) {
    if (!initialized_ || socket_fd_ < 0) {
        return false;
    }

    std::string serialized = msg.serialize();

    sockaddr_in peer_addr{};
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(static_cast<uint16_t>(peer_port_));
    inet_pton(AF_INET, peer_ip_.c_str(), &peer_addr.sin_addr);

    int sent = sendto(
        static_cast<socket_t>(socket_fd_),
        serialized.c_str(),
        static_cast<int>(serialized.length()),
        0,
        reinterpret_cast<sockaddr*>(&peer_addr),
        sizeof(peer_addr));

    return (sent > 0);
}

bool SimulatedCanChannel::receive(SecureCanMessage& out_msg, int timeout_ms) {
    if (!initialized_ || socket_fd_ < 0) {
        return false;
    }

    // Configure socket receive timeout
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(static_cast<socket_t>(socket_fd_), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(static_cast<socket_t>(socket_fd_), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    char buffer[2048];
    sockaddr_in sender_addr{};
#ifdef _WIN32
    int addr_len = sizeof(sender_addr);
#else
    socklen_t addr_len = sizeof(sender_addr);
#endif

    int bytes_read = recvfrom(
        static_cast<socket_t>(socket_fd_),
        buffer,
        sizeof(buffer) - 1,
        0,
        reinterpret_cast<sockaddr*>(&sender_addr),
        &addr_len);

    if (bytes_read <= 0) {
        return false;
    }

    buffer[bytes_read] = '\0';
    std::string payload_str(buffer, bytes_read);
    return SecureCanMessage::deserialize(payload_str, out_msg);
}

#ifdef __linux__
  #include <linux/can.h>
  #include <linux/can/raw.h>
  #include <net/if.h>
  #include <sys/ioctl.h>
#endif

// SocketCAN implementation for Linux Raspberry Pi 4 + MCP2515 (can0)
SocketCanChannel::SocketCanChannel(const std::string& iface_name)
    : iface_name_(iface_name)
{
}

SocketCanChannel::~SocketCanChannel() {
    close();
}

bool SocketCanChannel::init() {
#ifdef __linux__
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        std::cerr << "[SocketCanChannel] Failed to open PF_CAN raw socket: " << strerror(errno) << "\n";
        return false;
    }

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, iface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "[SocketCanChannel] ioctl SIOCGIFINDEX failed for interface " << iface_name_
                  << ": " << strerror(errno) << " (Ensure can0 is configured and up)\n";
        ::close(s);
        return false;
    }

    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[SocketCanChannel] Failed to bind to " << iface_name_ << ": " << strerror(errno) << "\n";
        ::close(s);
        return false;
    }

    socket_fd_ = s;
    initialized_ = true;
    std::cout << "[SocketCanChannel] Successfully bound to Linux SocketCAN interface: " << iface_name_ << "\n";
    return true;
#else
    std::cerr << "[SocketCanChannel] Real SocketCAN is supported on Linux (Raspberry Pi 4 / can0).\n"
              << "Host environment does not provide Linux AF_CAN kernel subsystem.\n";
    return false;
#endif
}

void SocketCanChannel::close() {
    if (socket_fd_ >= 0) {
        CLOSE_SOCKET(static_cast<socket_t>(socket_fd_));
        socket_fd_ = -1;
        initialized_ = false;
    }
}

bool SocketCanChannel::send(const SecureCanMessage& msg) {
#ifdef __linux__
    if (!initialized_ || socket_fd_ < 0) {
        return false;
    }

    std::vector<CanRawFrame> frames;
    if (!CanTransportProtocol::fragmentMessage(msg, frames)) {
        return false;
    }

    for (const auto& raw : frames) {
        struct can_frame frame{};
        frame.can_id = raw.can_id;
        frame.can_dlc = raw.can_dlc;
        std::memcpy(frame.data, raw.data, raw.can_dlc);

        ssize_t sent = write(socket_fd_, &frame, sizeof(struct can_frame));
        if (sent <= 0) {
            std::cerr << "[SocketCanChannel] Write error on " << iface_name_ << "\n";
            return false;
        }
    }
    return true;
#else
    (void)msg;
    return false;
#endif
}

bool SocketCanChannel::receive(SecureCanMessage& out_msg, int timeout_ms) {
#ifdef __linux__
    if (!initialized_ || socket_fd_ < 0) {
        return false;
    }

    struct pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) {
        return transport_.checkTimeout(out_msg);
    }

    struct can_frame frame{};
    ssize_t nbytes = read(socket_fd_, &frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return false;
        }
        std::cerr << "[SocketCanChannel] Socket read error: " << strerror(errno) << "\n";
        return false;
    }

    // Ignore RTR (Remote Transmission Request) frames
    if (frame.can_id & CAN_RTR_FLAG) {
        return false;
    }

    // Handle CAN bus error frames and detect bus-off
    if (frame.can_id & CAN_ERR_FLAG) {
        canid_t err_id = frame.can_id & CAN_ERR_MASK;
        if (err_id & CAN_ERR_BUSOFF) {
            std::cerr << "[SocketCanChannel] CRITICAL: CAN controller entered BUS-OFF. Recovery active.\n";
        } else {
            std::cerr << "[SocketCanChannel] CAN bus error frame detected (0x" << std::hex << err_id << std::dec << ")\n";
        }
        return false; // Do not pass error frame into reassembler
    }

    CanRawFrame raw{};
    raw.can_id = (frame.can_id & CAN_EFF_FLAG) ? (frame.can_id & CAN_EFF_MASK) : (frame.can_id & CAN_SFF_MASK);
    raw.can_dlc = frame.can_dlc;
    std::memcpy(raw.data, frame.data, std::min(static_cast<size_t>(frame.can_dlc), size_t(8)));
    raw.timestamp_us = getCurrentTimestampMicros();

    return transport_.processIncomingFrame(raw, out_msg);
#else
    (void)out_msg;
    (void)timeout_ms;
    return false;
#endif
}

bool SocketCanChannel::feedRawFrame(const CanRawFrame& frame, SecureCanMessage& out_msg) {
    return transport_.processIncomingFrame(frame, out_msg);
}

} // namespace autoshieldx
