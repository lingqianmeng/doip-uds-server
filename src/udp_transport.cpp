#include "doip_uds/udp_transport.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

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
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace doip_uds {
namespace {

constexpr std::size_t kMaximumUdpPayloadSize = 65507;
constexpr auto kReceivePollInterval = std::chrono::milliseconds(100);

int last_socket_error() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void throw_socket_error(const char* operation, int error) {
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

struct UdpListener::Impl {
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

        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket == kInvalidSocket) {
            throw_socket_error("socket", last_socket_error());
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(requested_port);
        const int parse_result =
            inet_pton(AF_INET, bind_address.c_str(), &address.sin_addr);
        if (parse_result != 1) {
            throw std::invalid_argument("bind address must be a valid IPv4 address");
        }

        if (::bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            throw_socket_error("bind", last_socket_error());
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

    std::optional<UdpDatagram> receive(std::optional<std::chrono::milliseconds> timeout_duration) {
        std::vector<std::uint8_t> payload(kMaximumUdpPayloadSize);
        const auto deadline = timeout_duration
                                  ? std::optional(std::chrono::steady_clock::now() +
                                                  *timeout_duration)
                                  : std::nullopt;
        while (!stopped.load()) {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(socket, &read_set);
            timeval timeout{};
            auto wait_duration = kReceivePollInterval;
            if (deadline) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    *deadline - std::chrono::steady_clock::now());
                wait_duration = std::max(std::chrono::milliseconds::zero(),
                                          std::min(wait_duration, remaining));
            }
            timeout.tv_sec = static_cast<long>(wait_duration.count() / 1000);
            timeout.tv_usec = static_cast<long>((wait_duration.count() % 1000) * 1000);

#ifdef _WIN32
            const int ready = select(0, &read_set, nullptr, nullptr, &timeout);
#else
            const int ready = select(socket + 1, &read_set, nullptr, nullptr, &timeout);
#endif
            if (ready == 0) {
                if (deadline && std::chrono::steady_clock::now() >= *deadline) {
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

            sockaddr_in sender{};
#ifdef _WIN32
            int sender_size = sizeof(sender);
            const int received = recvfrom(socket,
                                          reinterpret_cast<char*>(payload.data()),
                                          static_cast<int>(payload.size()),
                                          0,
                                          reinterpret_cast<sockaddr*>(&sender),
                                          &sender_size);
#else
            socklen_t sender_size = sizeof(sender);
            const auto received = recvfrom(socket,
                                           payload.data(),
                                           payload.size(),
                                           0,
                                           reinterpret_cast<sockaddr*>(&sender),
                                           &sender_size);
#endif
            if (received < 0) {
                const int error = last_socket_error();
#ifndef _WIN32
                if (error == EINTR) {
                    continue;
                }
#endif
                throw_socket_error("recvfrom", error);
            }

            char sender_address[INET_ADDRSTRLEN]{};
            if (inet_ntop(AF_INET, &sender.sin_addr, sender_address, sizeof(sender_address)) ==
                nullptr) {
                throw_socket_error("inet_ntop", last_socket_error());
            }
            payload.resize(static_cast<std::size_t>(received));
            return UdpDatagram{std::move(payload), sender_address, ntohs(sender.sin_port)};
        }
        return std::nullopt;
    }

    void send_to(const std::string& destination_address,
                 std::uint16_t destination_port,
                 const std::vector<std::uint8_t>& payload) {
        if (payload.size() > kMaximumUdpPayloadSize) {
            throw std::invalid_argument("UDP payload exceeds the maximum IPv4 datagram size");
        }

        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(destination_port);
        if (inet_pton(AF_INET, destination_address.c_str(), &destination.sin_addr) != 1) {
            throw std::invalid_argument("destination address must be a valid IPv4 address");
        }

#ifdef _WIN32
        const int sent = ::sendto(socket,
                                  reinterpret_cast<const char*>(payload.data()),
                                  static_cast<int>(payload.size()),
                                  0,
                                  reinterpret_cast<const sockaddr*>(&destination),
                                  sizeof(destination));
#else
        const auto sent = ::sendto(socket,
                                   payload.data(),
                                   payload.size(),
                                   0,
                                   reinterpret_cast<const sockaddr*>(&destination),
                                   sizeof(destination));
#endif
        if (sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
            throw_socket_error("sendto", last_socket_error());
        }
    }
};

UdpListener::UdpListener(const std::string& bind_address, std::uint16_t port)
    : impl_(std::make_unique<Impl>()) {
    impl_->open(bind_address, port);
}

UdpListener::~UdpListener() = default;

std::optional<UdpDatagram> UdpListener::receive() {
    return impl_->receive(std::nullopt);
}

std::optional<UdpDatagram> UdpListener::receive_for(std::chrono::milliseconds timeout) {
    if (timeout < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("receive timeout cannot be negative");
    }
    return impl_->receive(timeout);
}

void UdpListener::send_to(const std::string& address,
                          std::uint16_t port,
                          const std::vector<std::uint8_t>& payload) {
    impl_->send_to(address, port, payload);
}

void UdpListener::shutdown() noexcept {
    impl_->stopped.store(true);
}

std::uint16_t UdpListener::port() const noexcept {
    return impl_->bound_port;
}

}  // namespace doip_uds
