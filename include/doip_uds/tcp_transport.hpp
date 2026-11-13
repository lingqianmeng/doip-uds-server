#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace doip_uds {

inline constexpr std::uint16_t kDoipTcpPort = 13400;

class TcpConnection {
public:
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&) noexcept;
    TcpConnection& operator=(TcpConnection&&) noexcept;

    /// Returns false after the peer closes and all queued bytes have been read.
    /// Incoming bytes are only peeked at and remain unread for later processing.
    bool is_connected() const;

    /// Returns the remote IPv4 address and TCP source port.
    const std::string& peer_address() const noexcept;
    std::uint16_t peer_port() const noexcept;

private:
    struct Impl;
    explicit TcpConnection(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;

    friend class TcpListener;
};

class TcpListener {
public:
    explicit TcpListener(const std::string& bind_address = "0.0.0.0",
                         std::uint16_t port = kDoipTcpPort);
    ~TcpListener();

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener(TcpListener&&) = delete;
    TcpListener& operator=(TcpListener&&) = delete;

    /// Waits up to timeout for one client. Returns nullopt on timeout or shutdown.
    /// Socket failures throw std::system_error.
    std::optional<TcpConnection> accept_for(std::chrono::milliseconds timeout);

    /// Requests shutdown; a blocked accept_for returns within approximately 100 ms.
    void shutdown() noexcept;

    /// Returns the bound port, including the assigned port when constructed with port zero.
    std::uint16_t port() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace doip_uds
