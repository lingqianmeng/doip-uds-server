#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/diagnostic_message.hpp"
#include "doip_uds/doip_header.hpp"
#include "doip_uds/tcp_transport.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
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
            throw std::runtime_error("Failed to create diagnostic-message test socket");
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
            throw std::runtime_error("Failed to connect to diagnostic-message test listener");
        }
    }

    void send_bytes(const std::vector<std::uint8_t>& bytes) {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
#ifdef _WIN32
            const int sent = send(socket_,
                                  reinterpret_cast<const char*>(bytes.data() + offset),
                                  static_cast<int>(bytes.size() - offset),
                                  0);
#else
            const auto sent = send(socket_, bytes.data() + offset, bytes.size() - offset, 0);
#endif
            if (sent <= 0) {
                throw std::runtime_error("Failed to send diagnostic-message test bytes");
            }
            offset += static_cast<std::size_t>(sent);
        }
    }

    std::vector<std::uint8_t> receive_bytes(std::size_t size,
                                            std::chrono::milliseconds timeout) {
        std::vector<std::uint8_t> bytes(size);
        std::size_t offset = 0;
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (offset < size) {
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= std::chrono::microseconds::zero()) {
                throw std::runtime_error("Timed out receiving diagnostic-message test bytes");
            }

            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(socket_, &read_set);
            timeval wait_time{};
            wait_time.tv_sec = static_cast<long>(remaining.count() / 1000000);
            wait_time.tv_usec = static_cast<long>(remaining.count() % 1000000);
#ifdef _WIN32
            const int ready = select(0, &read_set, nullptr, nullptr, &wait_time);
#else
            const int ready = select(socket_ + 1, &read_set, nullptr, nullptr, &wait_time);
#endif
            if (ready <= 0) {
                throw std::runtime_error("Timed out receiving diagnostic-message test bytes");
            }

#ifdef _WIN32
            const int received = recv(socket_,
                                      reinterpret_cast<char*>(bytes.data() + offset),
                                      static_cast<int>(size - offset),
                                      0);
#else
            const auto received = recv(socket_, bytes.data() + offset, size - offset, 0);
#endif
            if (received <= 0) {
                throw std::runtime_error("Connection closed before diagnostic response");
            }
            offset += static_cast<std::size_t>(received);
        }
        return bytes;
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
    const auto header = doip_uds::encode_doip_header(
        {doip_uds::kDoipProtocolVersion,
         payload_type,
         static_cast<std::uint32_t>(payload.size())});
    std::vector<std::uint8_t> message(header.size() + payload.size());
    std::copy(header.begin(), header.end(), message.begin());
    std::copy(payload.begin(), payload.end(), message.begin() + header.size());
    return message;
}

struct ReceivedResponse {
    std::uint16_t payload_type{};
    std::vector<std::uint8_t> payload;
};

ReceivedResponse exchange_diagnostic_message(
    const doip_uds::DiagnosticMessage& request,
    std::optional<std::uint16_t> active_tester_logical_address) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto server_connection = listener.accept_for(std::chrono::seconds(1));
    if (!server_connection) {
        throw std::runtime_error("TCP listener did not accept the test client");
    }

    client.send_bytes(make_message(doip_uds::kDoipPayloadTypeDiagnosticMessage,
                                   doip_uds::encode_diagnostic_message(request)));
    const auto received_request =
        server_connection->receive_message_for(std::chrono::seconds(1));
    if (!received_request) {
        throw std::runtime_error("Server connection did not receive the diagnostic message");
    }
    const auto request_header =
        doip_uds::decode_doip_header(received_request->data(), received_request->size());
    if (!request_header ||
        request_header->payload_type != doip_uds::kDoipPayloadTypeDiagnosticMessage) {
        throw std::runtime_error("Received an invalid diagnostic-message frame");
    }

    const auto result = doip_uds::handle_diagnostic_message(
        received_request->data() + doip_uds::kDoipGenericHeaderSize,
        static_cast<std::size_t>(request_header->payload_length),
        active_tester_logical_address,
        0x1001);
    server_connection->send_message(result.response_payload_type, result.response_payload);

    const auto response_header_bytes =
        client.receive_bytes(doip_uds::kDoipGenericHeaderSize, std::chrono::seconds(1));
    const auto response_header =
        doip_uds::decode_doip_header(response_header_bytes.data(), response_header_bytes.size());
    if (!response_header) {
        throw std::runtime_error("Received an invalid diagnostic response header");
    }
    const auto response_payload = client.receive_bytes(
        static_cast<std::size_t>(response_header->payload_length), std::chrono::seconds(1));
    return {response_header->payload_type, response_payload};
}

}  // namespace

TEST(DiagnosticMessageTcp, SendsNegativeAcknowledgementBeforeActivation) {
    const auto response =
        exchange_diagnostic_message({0x0E00, 0x1001, {0x3E, 0x00}}, std::nullopt);

    EXPECT_EQ(response.payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessageNegativeAck);
    EXPECT_EQ(response.payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x02}));
}

TEST(DiagnosticMessageTcp, SendsNegativeAcknowledgementForInvalidTarget) {
    const auto response =
        exchange_diagnostic_message({0x0E00, 0x1002, {0x3E, 0x00}}, 0x0E00);

    EXPECT_EQ(response.payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessageNegativeAck);
    EXPECT_EQ(response.payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x03}));
}

TEST(DiagnosticMessageTcp, SendsPositiveAcknowledgementForValidMessage) {
    const auto response =
        exchange_diagnostic_message({0x0E00, 0x1001, {0x22, 0xF1, 0x90}}, 0x0E00);

    EXPECT_EQ(response.payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessagePositiveAck);
    EXPECT_EQ(response.payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x00}));
}
