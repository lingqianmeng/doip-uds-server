#pragma once

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

    /// Requests shutdown; a blocked receive returns within approximately 100 ms.
    void shutdown() noexcept;

    /// Returns the bound port, including the assigned port when constructed with port zero.
    std::uint16_t port() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace doip_uds
