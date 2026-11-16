#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/diagnostic_message.hpp"
#include "doip_uds/doip_header.hpp"
#include "doip_uds/routing_activation.hpp"
#include "doip_uds/tcp_transport.hpp"
#include "doip_uds/uds_dispatcher.hpp"

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
            throw std::runtime_error("Failed to create diagnostic-flow test socket");
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
            throw std::runtime_error("Failed to connect to diagnostic-flow test listener");
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
                throw std::runtime_error("Failed to send diagnostic-flow test bytes");
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
                throw std::runtime_error("Timed out receiving diagnostic-flow test bytes");
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
                throw std::runtime_error("Timed out receiving diagnostic-flow test bytes");
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
                throw std::runtime_error("Connection closed before diagnostic-flow response");
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

struct DoipFrame {
    std::uint16_t payload_type{};
    std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> make_message(std::uint16_t payload_type,
                                       const std::vector<std::uint8_t>& payload) {
    const auto header = doip_uds::encode_doip_header(
        {doip_uds::kDoipProtocolVersion,
         payload_type,
         static_cast<std::uint32_t>(payload.size())});
    std::vector<std::uint8_t> message(header.begin(), header.end());
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

DoipFrame receive_frame(ClientSocket& client) {
    const auto header_bytes =
        client.receive_bytes(doip_uds::kDoipGenericHeaderSize, std::chrono::seconds(1));
    const auto header =
        doip_uds::decode_doip_header(header_bytes.data(), header_bytes.size());
    if (!header) {
        throw std::runtime_error("Received an invalid DoIP response header");
    }
    return {header->payload_type,
            client.receive_bytes(static_cast<std::size_t>(header->payload_length),
                                 std::chrono::seconds(1))};
}

}  // namespace

TEST(DiagnosticFlowTcp, ActivatesAcknowledgesAndReturnsUdsResponseOnSameConnection) {
    constexpr std::uint16_t kTesterAddress = 0x0E00;
    constexpr std::uint16_t kEntityAddress = 0x1001;
    const doip_uds::DiagnosticMessage diagnostic_request{
        kTesterAddress, kEntityAddress, {0x3E, 0x00}};

    doip_uds::TcpListener listener("127.0.0.1", 0);
    ClientSocket client;
    client.connect_to(listener.port());
    auto server_connection = listener.accept_for(std::chrono::seconds(1));
    ASSERT_TRUE(server_connection.has_value());

    const doip_uds::RoutingActivationRequest activation_request{
        kTesterAddress, doip_uds::kDoipDefaultRoutingActivationType, std::nullopt};
    client.send_bytes(make_message(doip_uds::kDoipPayloadTypeRoutingActivationRequest,
                                   doip_uds::encode_routing_activation_request(
                                       activation_request)));

    const auto received_activation =
        server_connection->receive_message_for(std::chrono::seconds(1));
    ASSERT_TRUE(received_activation.has_value());
    const auto activation_header = doip_uds::decode_doip_header(
        received_activation->data(), received_activation->size());
    ASSERT_TRUE(activation_header.has_value());
    ASSERT_EQ(activation_header->payload_type,
              doip_uds::kDoipPayloadTypeRoutingActivationRequest);

    const auto decoded_activation = doip_uds::decode_routing_activation_request(
        received_activation->data() + doip_uds::kDoipGenericHeaderSize,
        static_cast<std::size_t>(activation_header->payload_length));
    ASSERT_TRUE(decoded_activation.has_value());
    doip_uds::RoutingActivationSession activation_session;
    const auto activation_response = activation_session.handle_request(
        *decoded_activation, kEntityAddress, false);
    server_connection->send_message(
        doip_uds::kDoipPayloadTypeRoutingActivationResponse,
        doip_uds::encode_routing_activation_response(activation_response));

    const auto received_activation_response = receive_frame(client);
    ASSERT_EQ(received_activation_response.payload_type,
              doip_uds::kDoipPayloadTypeRoutingActivationResponse);
    const auto decoded_activation_response =
        doip_uds::decode_routing_activation_response(
            received_activation_response.payload.data(),
            received_activation_response.payload.size());
    ASSERT_TRUE(decoded_activation_response.has_value());
    EXPECT_EQ(decoded_activation_response->response_code,
              doip_uds::RoutingActivationResponseCode::routing_successfully_activated);

    client.send_bytes(make_message(doip_uds::kDoipPayloadTypeDiagnosticMessage,
                                   doip_uds::encode_diagnostic_message(diagnostic_request)));
    const auto received_diagnostic =
        server_connection->receive_message_for(std::chrono::seconds(1));
    ASSERT_TRUE(received_diagnostic.has_value());
    const auto diagnostic_header = doip_uds::decode_doip_header(
        received_diagnostic->data(), received_diagnostic->size());
    ASSERT_TRUE(diagnostic_header.has_value());
    ASSERT_EQ(diagnostic_header->payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessage);

    const auto diagnostic_result = doip_uds::handle_diagnostic_message(
        received_diagnostic->data() + doip_uds::kDoipGenericHeaderSize,
        static_cast<std::size_t>(diagnostic_header->payload_length),
        activation_session.active_tester_logical_address(),
        kEntityAddress);
    ASSERT_TRUE(diagnostic_result.accepted_message.has_value());
    EXPECT_EQ(diagnostic_result.accepted_message->user_data, diagnostic_request.user_data);
    server_connection->send_message(diagnostic_result.response_payload_type,
                                    diagnostic_result.response_payload);

    const auto received_acknowledgement = receive_frame(client);
    EXPECT_EQ(received_acknowledgement.payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessagePositiveAck);
    EXPECT_EQ(received_acknowledgement.payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x00}));

    doip_uds::UdsConnectionState uds_state;
    const auto uds_result = doip_uds::dispatch_uds_request(
        diagnostic_result.accepted_message->user_data, uds_state);
    ASSERT_TRUE(uds_result.response.has_value());
    server_connection->send_message(
        doip_uds::kDoipPayloadTypeDiagnosticMessage,
        doip_uds::encode_diagnostic_message(
            {kEntityAddress, kTesterAddress, *uds_result.response}));

    const auto received_uds_response = receive_frame(client);
    ASSERT_EQ(received_uds_response.payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessage);
    const auto decoded_uds_response = doip_uds::decode_diagnostic_message(
        received_uds_response.payload.data(), received_uds_response.payload.size());
    ASSERT_TRUE(decoded_uds_response.has_value());
    EXPECT_EQ(decoded_uds_response->source_logical_address, kEntityAddress);
    EXPECT_EQ(decoded_uds_response->target_logical_address, kTesterAddress);
    EXPECT_EQ(decoded_uds_response->user_data, (std::vector<std::uint8_t>{0x7E, 0x00}));
}
