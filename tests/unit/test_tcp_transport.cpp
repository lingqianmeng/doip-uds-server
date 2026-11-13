#include "doip_uds/tcp_transport.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "doip_uds/udp_transport.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using TestSocket = int;
constexpr TestSocket kInvalidTestSocket = -1;
#endif

namespace {

void close_test_socket(TestSocket socket) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

class ClientSocket {
public:
    ClientSocket() : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
        if (socket_ == kInvalidTestSocket) {
            throw std::runtime_error("Failed to create TCP transport-test socket");
        }
    }

    ~ClientSocket() {
        close();
    }

    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;

    void connect_to(std::uint16_t port) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
            throw std::runtime_error("Failed to parse loopback address");
        }
        if (::connect(socket_,
                      reinterpret_cast<const sockaddr*>(&address),
                      sizeof(address)) != 0) {
            throw std::runtime_error("Failed to connect to TCP transport-test listener");
        }
    }

    void close() noexcept {
        if (socket_ != kInvalidTestSocket) {
            close_test_socket(socket_);
            socket_ = kInvalidTestSocket;
        }
    }

private:
    TestSocket socket_;
};

}  // namespace

TEST(TcpListener, BindsEphemeralPortAndAcceptsConnection) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ASSERT_NE(listener.port(), 0);

    ClientSocket client;
    client.connect_to(listener.port());
    auto accepted = listener.accept_for(std::chrono::seconds(1));

    ASSERT_TRUE(accepted.has_value());
    EXPECT_TRUE(accepted->is_connected());
    EXPECT_EQ(accepted->peer_address(), "127.0.0.1");
    EXPECT_NE(accepted->peer_port(), 0);
}

TEST(TcpListener, CanSharePortNumberWithUdpListener) {
    doip_uds::UdpListener udp_listener("127.0.0.1", 0);
    doip_uds::TcpListener tcp_listener("127.0.0.1", udp_listener.port());

    EXPECT_EQ(tcp_listener.port(), udp_listener.port());
}

TEST(TcpListener, AcceptForReturnsNulloptAfterTimeout) {
    doip_uds::TcpListener listener("127.0.0.1", 0);

    EXPECT_FALSE(listener.accept_for(std::chrono::milliseconds(20)).has_value());
}

TEST(TcpListener, ShutdownInterruptsWaitingAccept) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    std::optional<doip_uds::TcpConnection> accepted;
    std::thread acceptor([&listener, &accepted] {
        accepted = listener.accept_for(std::chrono::seconds(2));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    listener.shutdown();
    acceptor.join();

    EXPECT_FALSE(accepted.has_value());
}

TEST(TcpListener, TracksConcurrentClientsUntilEachDisconnects) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket first_client;
    ClientSocket second_client;
    first_client.connect_to(listener.port());
    second_client.connect_to(listener.port());

    auto first_connection = listener.accept_for(std::chrono::seconds(1));
    auto second_connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(first_connection.has_value());
    ASSERT_TRUE(second_connection.has_value());
    ASSERT_TRUE(first_connection->is_connected());
    ASSERT_TRUE(second_connection->is_connected());

    first_client.close();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (first_connection->is_connected() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_FALSE(first_connection->is_connected());
    EXPECT_TRUE(second_connection->is_connected());
}

TEST(TcpListener, RejectsInvalidBindAddress) {
    EXPECT_THROW(doip_uds::TcpListener("not-an-ip-address", 0), std::invalid_argument);
}

TEST(TcpListener, RejectsNegativeAcceptTimeout) {
    doip_uds::TcpListener listener("127.0.0.1", 0);

    EXPECT_THROW(listener.accept_for(std::chrono::milliseconds(-1)), std::invalid_argument);
}
