#include "doip_uds/tcp_transport.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/doip_header.hpp"
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

    void send_bytes(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::size_t length) {
        while (length > 0) {
#ifdef _WIN32
            const int sent = send(socket_,
                                  reinterpret_cast<const char*>(bytes.data() + offset),
                                  static_cast<int>(length),
                                  0);
#else
            const auto sent = send(socket_, bytes.data() + offset, length, 0);
#endif
            if (sent <= 0) {
                throw std::runtime_error("Failed to send TCP transport-test bytes");
            }
            const auto sent_size = static_cast<std::size_t>(sent);
            offset += sent_size;
            length -= sent_size;
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

std::vector<std::uint8_t> make_message(std::uint16_t payload_type,
                                       const std::vector<std::uint8_t>& payload) {
    const doip_uds::DoipHeader header{
        doip_uds::kDoipProtocolVersion,
        payload_type,
        static_cast<std::uint32_t>(payload.size())};
    const auto encoded_header = doip_uds::encode_doip_header(header);
    std::vector<std::uint8_t> message(encoded_header.begin(), encoded_header.end());
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

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

TEST(TcpConnection, ReceivesMessageSplitAcrossHeaderAndPayload) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(connection.has_value());

    const auto message = make_message(0x8001, {0x10, 0x20, 0x30, 0x40});
    client.send_bytes(message, 0, 3);
    EXPECT_FALSE(connection->receive_message_for(std::chrono::milliseconds(20)).has_value());

    client.send_bytes(message, 3, 7);
    EXPECT_FALSE(connection->receive_message_for(std::chrono::milliseconds(20)).has_value());

    client.send_bytes(message, 10, message.size() - 10);
    const auto received = connection->receive_message_for(std::chrono::seconds(1));
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, message);
}

TEST(TcpConnection, SeparatesMultipleMessagesReceivedTogether) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(connection.has_value());

    const auto first_message = make_message(0x8001, {0xAA, 0xBB});
    const auto second_message = make_message(0x8002, {});
    auto combined = first_message;
    combined.insert(combined.end(), second_message.begin(), second_message.end());
    client.send_bytes(combined, 0, combined.size());

    const auto first_received = connection->receive_message_for(std::chrono::seconds(1));
    const auto second_received = connection->receive_message_for(std::chrono::milliseconds(0));
    ASSERT_TRUE(first_received.has_value());
    ASSERT_TRUE(second_received.has_value());
    EXPECT_EQ(*first_received, first_message);
    EXPECT_EQ(*second_received, second_message);
}

TEST(TcpConnection, RejectsInvalidGenericHeader) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(connection.has_value());

    auto invalid_header = doip_uds::encode_doip_header(
        {doip_uds::kDoipProtocolVersion, 0x8001, 0});
    invalid_header[1] ^= 0x01;
    const std::vector<std::uint8_t> invalid_message(
        invalid_header.begin(), invalid_header.end());
    client.send_bytes(invalid_message, 0, invalid_message.size());

    EXPECT_THROW(connection->receive_message_for(std::chrono::seconds(1)),
                 std::runtime_error);
}

TEST(TcpConnection, RejectsPayloadLengthAboveConfiguredLimit) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(connection.has_value());

    const auto header = doip_uds::encode_doip_header(
        {doip_uds::kDoipProtocolVersion,
         0x8001,
         static_cast<std::uint32_t>(doip_uds::kMaxDoipTcpPayloadSize + 1)});
    const std::vector<std::uint8_t> oversized_header(header.begin(), header.end());
    client.send_bytes(oversized_header, 0, oversized_header.size());

    EXPECT_THROW(connection->receive_message_for(std::chrono::seconds(1)),
                 std::length_error);
}

TEST(TcpConnection, RejectsPeerCloseDuringIncompleteMessage) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(connection.has_value());

    const auto message = make_message(0x8001, {0x10, 0x20});
    client.send_bytes(message, 0, 5);
    EXPECT_FALSE(connection->receive_message_for(std::chrono::milliseconds(20)).has_value());
    client.close();

    EXPECT_THROW(connection->receive_message_for(std::chrono::seconds(1)),
                 std::runtime_error);
}

TEST(TcpListener, RejectsInvalidBindAddress) {
    EXPECT_THROW(doip_uds::TcpListener("not-an-ip-address", 0), std::invalid_argument);
}

TEST(TcpListener, RejectsNegativeAcceptTimeout) {
    doip_uds::TcpListener listener("127.0.0.1", 0);

    EXPECT_THROW(listener.accept_for(std::chrono::milliseconds(-1)), std::invalid_argument);
}
