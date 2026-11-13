#include "doip_uds/tcp_transport.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace doip_uds {
namespace {

constexpr auto kAcceptPollInterval = std::chrono::milliseconds(100);

int last_socket_error() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

[[noreturn]] void throw_socket_error(const char* operation, int error) {
    throw std::system_error(error, std::system_category(), operation);
}

void close_socket(SocketHandle socket) noexcept {
    if (socket == kInvalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

}  // namespace

struct TcpConnection::Impl {
    SocketHandle socket{kInvalidSocket};
    std::string remote_address;
    std::uint16_t remote_port{};
#ifdef _WIN32
    bool winsock_started{false};
#endif

    ~Impl() {
        close_socket(socket);
#ifdef _WIN32
        if (winsock_started) {
            WSACleanup();
        }
#endif
    }

    void initialize_winsock() {
#ifdef _WIN32
        WSADATA data{};
        const int startup_result = WSAStartup(MAKEWORD(2, 2), &data);
        if (startup_result != 0) {
            throw_socket_error("WSAStartup", startup_result);
        }
        winsock_started = true;
#endif
    }
};

struct TcpListener::Impl {
    SocketHandle socket{kInvalidSocket};
    std::uint16_t bound_port{};
    std::atomic<bool> stopped{false};
#ifdef _WIN32
    bool winsock_started{false};
#endif

    ~Impl() {
        close_socket(socket);
#ifdef _WIN32
        if (winsock_started) {
            WSACleanup();
        }
#endif
    }

    void open(const std::string& bind_address, std::uint16_t requested_port) {
#ifdef _WIN32
        WSADATA data{};
        const int startup_result = WSAStartup(MAKEWORD(2, 2), &data);
        if (startup_result != 0) {
            throw_socket_error("WSAStartup", startup_result);
        }
        winsock_started = true;
#endif

        socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == kInvalidSocket) {
            throw_socket_error("socket", last_socket_error());
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(requested_port);
        if (inet_pton(AF_INET, bind_address.c_str(), &address.sin_addr) != 1) {
            throw std::invalid_argument("bind address must be a valid IPv4 address");
        }

        if (::bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            throw_socket_error("bind", last_socket_error());
        }
        if (::listen(socket, SOMAXCONN) != 0) {
            throw_socket_error("listen", last_socket_error());
        }

        sockaddr_in local_address{};
#ifdef _WIN32
        int address_size = sizeof(local_address);
#else
        socklen_t address_size = sizeof(local_address);
#endif
        if (getsockname(socket, reinterpret_cast<sockaddr*>(&local_address), &address_size) != 0) {
            throw_socket_error("getsockname", last_socket_error());
        }
        bound_port = ntohs(local_address.sin_port);
    }

    std::optional<TcpConnection> accept_for(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!stopped.load()) {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(socket, &read_set);

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            const auto wait_duration =
                std::max(std::chrono::milliseconds::zero(),
                         std::min(kAcceptPollInterval, remaining));
            timeval wait_time{};
            wait_time.tv_sec = static_cast<long>(wait_duration.count() / 1000);
            wait_time.tv_usec = static_cast<long>((wait_duration.count() % 1000) * 1000);

#ifdef _WIN32
            const int ready = select(0, &read_set, nullptr, nullptr, &wait_time);
#else
            const int ready = select(socket + 1, &read_set, nullptr, nullptr, &wait_time);
#endif
            if (ready == 0) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    return std::nullopt;
                }
                continue;
            }
            if (ready < 0) {
#ifndef _WIN32
                if (errno == EINTR) {
                    continue;
                }
#endif
                throw_socket_error("select", last_socket_error());
            }
            if (stopped.load()) {
                return std::nullopt;
            }

            sockaddr_in peer{};
#ifdef _WIN32
            int peer_size = sizeof(peer);
#else
            socklen_t peer_size = sizeof(peer);
#endif
            const SocketHandle client =
                ::accept(socket, reinterpret_cast<sockaddr*>(&peer), &peer_size);
            if (client == kInvalidSocket) {
                const int error = last_socket_error();
#ifdef _WIN32
                if (error == WSAEWOULDBLOCK) {
                    continue;
                }
#else
                if (error == EINTR) {
                    continue;
                }
                if (error == EAGAIN || error == EWOULDBLOCK) {
                    continue;
                }
#endif
                if (stopped.load()) {
                    return std::nullopt;
                }
                throw_socket_error("accept", error);
            }

            auto connection = std::make_unique<TcpConnection::Impl>();
            connection->socket = client;
            connection->initialize_winsock();
            char peer_address[INET_ADDRSTRLEN]{};
            if (inet_ntop(AF_INET, &peer.sin_addr, peer_address, sizeof(peer_address)) == nullptr) {
                throw_socket_error("inet_ntop", last_socket_error());
            }
            connection->remote_address = peer_address;
            connection->remote_port = ntohs(peer.sin_port);
            return TcpConnection(std::move(connection));
        }
        return std::nullopt;
    }
};

TcpConnection::TcpConnection(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

TcpConnection::~TcpConnection() = default;

TcpConnection::TcpConnection(TcpConnection&&) noexcept = default;

TcpConnection& TcpConnection::operator=(TcpConnection&&) noexcept = default;

bool TcpConnection::is_connected() const {
    if (!impl_ || impl_->socket == kInvalidSocket) {
        return false;
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(impl_->socket, &read_set);
    timeval no_wait{};
#ifdef _WIN32
    const int ready = select(0, &read_set, nullptr, nullptr, &no_wait);
#else
    const int ready = select(impl_->socket + 1, &read_set, nullptr, nullptr, &no_wait);
#endif
    if (ready == 0) {
        return true;
    }
    if (ready < 0) {
#ifndef _WIN32
        if (errno == EINTR) {
            return true;
        }
#endif
        throw_socket_error("select", last_socket_error());
    }

    char byte{};
#ifdef _WIN32
    const int received = recv(impl_->socket, &byte, 1, MSG_PEEK);
#else
    const auto received = recv(impl_->socket, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
#endif
    if (received > 0) {
        return true;
    }
    if (received == 0) {
        return false;
    }

    const int error = last_socket_error();
#ifdef _WIN32
    if (error == WSAEWOULDBLOCK) {
        return true;
    }
    if (error == WSAECONNRESET || error == WSAENOTCONN || error == WSAESHUTDOWN) {
        return false;
    }
#else
    if (error == EAGAIN || error == EWOULDBLOCK || error == EINTR) {
        return true;
    }
    if (error == ECONNRESET || error == ENOTCONN) {
        return false;
    }
#endif
    throw_socket_error("recv(MSG_PEEK)", error);
}

const std::string& TcpConnection::peer_address() const noexcept {
    static const std::string empty;
    return impl_ ? impl_->remote_address : empty;
}

std::uint16_t TcpConnection::peer_port() const noexcept {
    return impl_ ? impl_->remote_port : 0;
}

TcpListener::TcpListener(const std::string& bind_address, std::uint16_t port)
    : impl_(std::make_unique<Impl>()) {
    impl_->open(bind_address, port);
}

TcpListener::~TcpListener() = default;

std::optional<TcpConnection> TcpListener::accept_for(std::chrono::milliseconds timeout) {
    if (timeout < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("accept timeout cannot be negative");
    }
    return impl_->accept_for(timeout);
}

void TcpListener::shutdown() noexcept {
    impl_->stopped.store(true);
}

std::uint16_t TcpListener::port() const noexcept {
    return impl_->bound_port;
}

}  // namespace doip_uds
