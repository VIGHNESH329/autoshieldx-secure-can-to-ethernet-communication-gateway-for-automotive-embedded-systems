#pragma once

#include "autoshieldx/can/can_frame.hpp"
#include "autoshieldx/can/can_transport.hpp"
#include <memory>
#include <string>
#include <functional>

namespace autoshieldx {

/**
 * @brief Abstract CAN Communication Channel Interface
 *
 * Decouples the upper security layers from physical transport.
 * Allows effortless substitution of SimulatedCanChannel with
 * SocketCanChannel (MCP2515 hardware) without altering a single line
 * of the gateway security pipeline.
 */
class ICanChannel {
public:
    virtual ~ICanChannel() = default;

    virtual bool init() = 0;
    virtual void close() = 0;
    virtual bool send(const SecureCanMessage& msg) = 0;
    virtual bool receive(SecureCanMessage& out_msg, int timeout_ms = 500) = 0;
    virtual std::string getChannelType() const = 0;
    virtual bool isHardware() const = 0;
};

/**
 * @brief Simulated UDP/Loopback CAN Channel
 *
 * Runs locally on localhost without requiring root privileges,
 * virtual CAN drivers (vcan), or physical CAN transceivers.
 * Transmits serialized SecureCanMessage packets over UDP sockets.
 */
class SimulatedCanChannel : public ICanChannel {
public:
    SimulatedCanChannel(const std::string& bind_ip, int bind_port,
                        const std::string& peer_ip = "127.0.0.1", int peer_port = 15000);
    ~SimulatedCanChannel() override;

    bool init() override;
    void close() override;
    bool send(const SecureCanMessage& msg) override;
    bool receive(SecureCanMessage& out_msg, int timeout_ms = 500) override;
    std::string getChannelType() const override { return "Simulated_UDP_Loopback"; }
    bool isHardware() const override { return false; }

private:
    std::string bind_ip_;
    int bind_port_;
    std::string peer_ip_;
    int peer_port_;
    int socket_fd_{-1};
    bool initialized_{false};
};

/**
 * @brief SocketCAN Hardware Adapter (Documentation & Blueprint for Phase 2)
 *
 * In Phase 2, when connecting the physical Raspberry Pi 4 to MCP2515 over SPI:
 * 1. Linux Kernel module `mcp251x` registers network device `can0`
 * 2. Setup command: `ip link set can0 type can bitrate 500000 && ip link set can0 up`
 * 3. SocketCanChannel opens `AF_CAN, SOCK_RAW, CAN_RAW` socket and reads `struct can_frame`
 *
 * CURRENT STATUS: Simulation only. Real hardware calls are stubbed and documented.
 */
class SocketCanChannel : public ICanChannel {
public:
    explicit SocketCanChannel(const std::string& iface_name = "can0");
    ~SocketCanChannel() override;

    bool init() override;
    void close() override;
    bool send(const SecureCanMessage& msg) override;
    bool receive(SecureCanMessage& out_msg, int timeout_ms = 500) override;
    std::string getChannelType() const override { return "SocketCAN_MCP2515"; }
    bool isHardware() const override { return true; }
    const std::string& getInterfaceName() const { return iface_name_; }

    // Direct interface for feeding raw CAN frames (e.g. for testing and simulation verification)
    bool feedRawFrame(const CanRawFrame& frame, SecureCanMessage& out_msg);

private:
    std::string iface_name_;
    int socket_fd_{-1};
    bool initialized_{false};
    CanTransportProtocol transport_;
};

} // namespace autoshieldx
