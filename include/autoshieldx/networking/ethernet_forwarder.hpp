#pragma once

#include "autoshieldx/can/can_frame.hpp"
#include <string>
#include <cstdint>
#include <memory>

#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <netinet/in.h>
#endif

namespace autoshieldx {


/**
 * @brief Ethernet Communication Forwarder Interface
 *
 * Implements the CAN-to-Ethernet gateway bridging mechanism.
 * When the gateway security pipeline marks a CAN message as VALID,
 * this module encapsulates it and transmits it across the Ethernet
 * vehicle backbone to the dashboard / telemetry receiver.
 */
class IEthernetForwarder {
public:
    virtual ~IEthernetForwarder() = default;

    virtual bool init() = 0;
    virtual void close() = 0;
    virtual bool forwardTelemetry(const SecureCanMessage& msg) = 0;
    virtual bool forwardSecurityAlert(const std::string& alert_json) = 0;
    virtual std::string getDestination() const = 0;
};

/**
 * @brief Localhost UDP/TCP Ethernet Forwarder
 *
 * Transmits JSON-formatted telemetry across loopback Ethernet (127.0.0.1:16000)
 * in simulation mode. Can be pointed to the actual Raspberry Pi 4 Ethernet IP
 * when hardware testing begins.
 */
class EthernetForwarder : public IEthernetForwarder {
public:
    EthernetForwarder(const std::string& target_ip = "127.0.0.1", int target_port = 16000, bool enable_broadcast = false);
    ~EthernetForwarder() override;

    bool init() override;
    void close() override;
    bool forwardTelemetry(const SecureCanMessage& msg) override;
    bool forwardSecurityAlert(const std::string& alert_json) override;
    std::string getDestination() const override;

    void setDestination(const std::string& target_ip, int target_port);
    void setBroadcast(bool enable);
    bool isBroadcastEnabled() const { return broadcast_enabled_; }
    uint64_t getForwardedCount() const { return forwarded_count_; }

private:
    std::string target_ip_;
    int target_port_;
    int socket_fd_{-1};
    bool initialized_{false};
    bool broadcast_enabled_{false};
    bool is_target_broadcast_{false};
    uint64_t forwarded_count_{0};

    // Cached pre-resolved socket addresses
    sockaddr_in dest_addr_{};
    sockaddr_in broadcast_addr_{};

    bool resolveAddresses();
    std::string getSocketErrorString();
    bool sendRawString(const std::string& payload);
};

} // namespace autoshieldx

