#include "doip_uds/udp_transport.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

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

struct SentDatagram {
    std::uint16_t sender_port;
    TestSocket socket;

    SentDatagram(std::uint16_t source_port, TestSocket source_socket)
        : sender_port(source_port), socket(source_socket) {}

    ~SentDatagram() {
        close_test_socket(socket);
    }

    SentDatagram(const SentDatagram&) = delete;
    SentDatagram& operator=(const SentDatagram&) = delete;
};

SentDatagram send_to_loopback(std::uint16_t port, const std::vector<std::uint8_t>& payload) {
    const TestSocket socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == kInvalidTestSocket) {
        throw std::runtime_error("Failed to create UDP test socket");
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &destination.sin_addr) != 1) {
        close_test_socket(socket);
        throw std::runtime_error("Failed to parse loopback address");
    }

#ifdef _WIN32
    const int sent = sendto(socket,
                            reinterpret_cast<const char*>(payload.data()),
                            static_cast<int>(payload.size()),
                            0,
                            reinterpret_cast<const sockaddr*>(&destination),
                            sizeof(destination));
#else
    const auto sent = sendto(socket,
                             payload.data(),
                             payload.size(),
                             0,
                             reinterpret_cast<const sockaddr*>(&destination),
                             sizeof(destination));
#endif
    if (sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
        close_test_socket(socket);
        throw std::runtime_error("Failed to send UDP test datagram");
    }

    sockaddr_in local_address{};
#ifdef _WIN32
    int local_address_size = sizeof(local_address);
#else
    socklen_t local_address_size = sizeof(local_address);
#endif
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&local_address), &local_address_size) != 0) {
        close_test_socket(socket);
        throw std::runtime_error("Failed to read UDP test sender port");
    }
    const auto sender_port = ntohs(local_address.sin_port);
    return SentDatagram{sender_port, socket};
}

}  // namespace

TEST(UdpListener, BindsEphemeralPortAndReceivesDatagramWithSender) {
    doip_uds::UdpListener listener("127.0.0.1", 0);
    ASSERT_NE(listener.port(), 0);

    const std::vector<std::uint8_t> expected_payload{0x02, 0xFD, 0x00, 0x01};
    const auto sent = send_to_loopback(listener.port(), expected_payload);
    const auto received = listener.receive();

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->payload, expected_payload);
    EXPECT_EQ(received->sender_address, "127.0.0.1");
    EXPECT_EQ(received->sender_port, sent.sender_port);
}

TEST(UdpListener, ReportsAddressAlreadyInUse) {
    doip_uds::UdpListener first("127.0.0.1", 0);

    try {
        doip_uds::UdpListener second("127.0.0.1", first.port());
        FAIL() << "Expected binding an occupied UDP port to fail";
    } catch (const std::system_error& error) {
        EXPECT_NE(std::string(error.what()).find("bind"), std::string::npos);
    }
}

TEST(UdpListener, RejectsInvalidBindAddress) {
    EXPECT_THROW(doip_uds::UdpListener("not-an-ip-address", 0), std::invalid_argument);
}

TEST(UdpListener, ShutdownInterruptsWaitingReceive) {
    doip_uds::UdpListener listener("127.0.0.1", 0);
    std::optional<doip_uds::UdpDatagram> received;
    std::thread receiver([&listener, &received] {
        received = listener.receive();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    listener.shutdown();
    receiver.join();

    EXPECT_FALSE(received.has_value());
    EXPECT_FALSE(listener.receive().has_value());
}
