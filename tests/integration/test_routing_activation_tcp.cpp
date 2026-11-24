#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/doip_header.hpp"
#include "doip_uds/routing_activation.hpp"
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
            throw std::runtime_error("Failed to create routing-activation test socket");
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
            throw std::runtime_error("Failed to connect to routing-activation test listener");
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
                throw std::runtime_error("Failed to send routing-activation test bytes");
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
                throw std::runtime_error("Timed out receiving routing-activation test bytes");
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
                throw std::runtime_error("Timed out receiving routing-activation test bytes");
            }

#ifdef _WIN32
            const int received = recv(socket_,
                                      reinterpret_cast<char*>(bytes.data() + offset),
                                      static_cast<int>(size - offset),
                                      0);
#else
            const auto received =
                recv(socket_, bytes.data() + offset, size - offset, 0);
#endif
            if (received <= 0) {
                throw std::runtime_error("Connection closed before routing-activation response");
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

doip_uds::RoutingActivationResponse exchange_routing_activation(
    const doip_uds::RoutingActivationRequest& request) {
    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto server_connection = listener.accept_for(std::chrono::seconds(1));
    if (!server_connection) {
        throw std::runtime_error("TCP listener did not accept the test client");
    }

    const auto request_payload = doip_uds::encode_routing_activation_request(request);
    client.send_bytes(
        make_message(doip_uds::kDoipPayloadTypeRoutingActivationRequest, request_payload));

    const auto received_request =
        server_connection->receive_message_for(std::chrono::seconds(1));
    if (!received_request) {
        throw std::runtime_error("Server connection did not receive the request");
    }
    const auto request_header =
        doip_uds::decode_doip_header(received_request->data(), received_request->size());
    if (!request_header ||
        request_header->payload_type != doip_uds::kDoipPayloadTypeRoutingActivationRequest) {
        throw std::runtime_error("Received an invalid routing-activation request frame");
    }
    const auto decoded_request = doip_uds::decode_routing_activation_request(
        received_request->data() + doip_uds::kDoipGenericHeaderSize,
        static_cast<std::size_t>(request_header->payload_length));
    if (!decoded_request) {
        throw std::runtime_error("Received an invalid routing-activation request payload");
    }

    doip_uds::RoutingActivationSession session;
    const auto response = session.handle_request(*decoded_request, 0x1001, false);
    server_connection->send_message(
        doip_uds::kDoipPayloadTypeRoutingActivationResponse,
        doip_uds::encode_routing_activation_response(response));

    const auto response_header_bytes = client.receive_bytes(
        doip_uds::kDoipGenericHeaderSize, std::chrono::seconds(1));
    const auto response_header =
        doip_uds::decode_doip_header(response_header_bytes.data(), response_header_bytes.size());
    if (!response_header ||
        response_header->payload_type != doip_uds::kDoipPayloadTypeRoutingActivationResponse) {
        throw std::runtime_error("Received an invalid routing-activation response frame");
    }
    const auto response_payload = client.receive_bytes(
        static_cast<std::size_t>(response_header->payload_length), std::chrono::seconds(1));
    const auto decoded_response =
        doip_uds::decode_routing_activation_response(response_payload.data(),
                                                     response_payload.size());
    if (!decoded_response) {
        throw std::runtime_error("Received an invalid routing-activation response payload");
    }
    return *decoded_response;
}

}  // namespace

TEST(RoutingActivationTcp, SendsSuccessResponseForDefaultActivation) {
    const auto response =
        exchange_routing_activation({0x0E00, 0x00, std::nullopt});

    EXPECT_EQ(response.tester_logical_address, 0x0E00);
    EXPECT_EQ(response.entity_logical_address, 0x1001);
    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::routing_successfully_activated);
}

TEST(RoutingActivationTcp, SendsRejectionResponseForUnsupportedActivation) {
    const auto response =
        exchange_routing_activation({0x0E00, 0x01, std::nullopt});

    EXPECT_EQ(response.tester_logical_address, 0x0E00);
    EXPECT_EQ(response.entity_logical_address, 0x1001);
    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::unsupported_activation_type);
}
