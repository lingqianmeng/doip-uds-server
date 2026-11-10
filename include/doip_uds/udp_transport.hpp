#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace doip_uds {

inline constexpr std::uint16_t kDoipUdpPort = 13400;

struct UdpDatagram {
    std::vector<std::uint8_t> payload;
    std::string sender_address;
    std::uint16_t sender_port{};
};

class UdpListener {
public:
    explicit UdpListener(const std::string& bind_address = "0.0.0.0",
                         std::uint16_t port = kDoipUdpPort);
    ~UdpListener();

    UdpListener(const UdpListener&) = delete;
    UdpListener& operator=(const UdpListener&) = delete;
    UdpListener(UdpListener&&) = delete;
    UdpListener& operator=(UdpListener&&) = delete;

    /// Receives one IPv4 UDP datagram. Returns nullopt after shutdown().
    /// Socket failures throw std::system_error.
    std::optional<UdpDatagram> receive();

    /// Waits up to timeout for one datagram. Returns nullopt on timeout or shutdown().
    std::optional<UdpDatagram> receive_for(std::chrono::milliseconds timeout);

    /// Sends one complete IPv4 UDP datagram to the specified peer.
    /// Invalid addresses throw std::invalid_argument; socket failures throw std::system_error.
    void send_to(const std::string& address,
                 std::uint16_t port,
                 const std::vector<std::uint8_t>& payload);

    /// Requests shutdown; a blocked receive returns within approximately 100 ms.
    void shutdown() noexcept;

    /// Returns the bound port, including the assigned port when constructed with port zero.
    std::uint16_t port() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace doip_uds
