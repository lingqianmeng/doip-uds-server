#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/doip_header.hpp"
#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/udp_transport.hpp"
#include "doip_uds/vehicle_identification.hpp"
#include "doip_uds/vehicle_identification_server.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using TestSocket = int;
constexpr TestSocket kInvalidTestSocket = -1;
#endif

namespace {

class ClientSocket {
public:
    ClientSocket() : socket_(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) {
        if (socket_ == kInvalidTestSocket) {
            throw std::runtime_error("Failed to create UDP integration-test socket");
        }

#ifdef _WIN32
        const DWORD timeout_ms = 3000;
        if (setsockopt(socket_,
                       SOL_SOCKET,
                       SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&timeout_ms),
                       sizeof(timeout_ms)) != 0) {
#else
        timeval timeout{};
        timeout.tv_sec = 3;
        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
#endif
            close();
            throw std::runtime_error("Failed to set UDP integration-test receive timeout");
        }
    }

    ~ClientSocket() {
        close();
    }

    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;

    TestSocket get() const noexcept {
        return socket_;
    }

private:
    void close() noexcept {
        if (socket_ == kInvalidTestSocket) {
            return;
        }
#ifdef _WIN32
        closesocket(socket_);
#else
        ::close(socket_);
#endif
        socket_ = kInvalidTestSocket;
    }

    TestSocket socket_;
};

std::vector<std::uint8_t> make_broadcast_request() {
    const doip_uds::DoipHeader header{
        doip_uds::kDoipDefaultProtocolVersionForVehicleIdentificationRequest,
        doip_uds::kDoipPayloadTypeVehicleIdentificationRequest,
        0};
    const auto encoded = doip_uds::encode_doip_header(header);
    return {encoded.begin(), encoded.end()};
}

bool send_request(TestSocket socket, std::uint16_t port) {
    const auto request = make_broadcast_request();
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &destination.sin_addr) != 1) {
        return false;
    }

#ifdef _WIN32
    const int sent = sendto(socket,
                            reinterpret_cast<const char*>(request.data()),
                            static_cast<int>(request.size()),
                            0,
                            reinterpret_cast<const sockaddr*>(&destination),
                            sizeof(destination));
#else
    const auto sent = sendto(socket,
                             request.data(),
                             request.size(),
                             0,
                             reinterpret_cast<const sockaddr*>(&destination),
                             sizeof(destination));
#endif
    return sent >= 0 && static_cast<std::size_t>(sent) == request.size();
}

}  // namespace

TEST(VehicleIdentificationUdpIntegration, BroadcastRequestReceivesIdentityResponse) {
    doip_uds::UdpListener listener("127.0.0.1", 0);
    ClientSocket client;
    const doip_uds::EcuIdentity identity{
        {'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'},
        0x1001,
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

    bool request_processed = false;
    std::exception_ptr server_error;
    std::thread server([&] {
        try {
            request_processed = doip_uds::process_next_vehicle_identification_request(
                listener, identity, std::chrono::seconds(2));
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    const bool request_sent = send_request(client.get(), listener.port());
    std::array<std::uint8_t,
               doip_uds::kDoipGenericHeaderSize +
                   doip_uds::kDoipVehicleIdentificationResponsePayloadSize>
        response_bytes{};
#ifdef _WIN32
    const int received = recv(client.get(),
                              reinterpret_cast<char*>(response_bytes.data()),
                              static_cast<int>(response_bytes.size()),
                              0);
#else
    const auto received =
        recv(client.get(), response_bytes.data(), response_bytes.size(), 0);
#endif
    server.join();

    ASSERT_TRUE(request_sent);
    if (server_error) {
        std::rethrow_exception(server_error);
    }
    ASSERT_TRUE(request_processed);
    ASSERT_GE(received, 0);
    ASSERT_EQ(static_cast<std::size_t>(received), response_bytes.size());

    const auto header =
        doip_uds::decode_doip_header(response_bytes.data(), response_bytes.size());
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->payload_type,
              doip_uds::kDoipPayloadTypeVehicleIdentificationResponse);
    EXPECT_EQ(header->payload_length, doip_uds::kDoipVehicleIdentificationResponsePayloadSize);

    const auto decoded = doip_uds::decode_vehicle_identification_response(
        response_bytes.data() + doip_uds::kDoipGenericHeaderSize,
        doip_uds::kDoipVehicleIdentificationResponsePayloadSize);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->vin, identity.vin);
    EXPECT_EQ(decoded->logical_address, identity.logical_address);
    EXPECT_EQ(decoded->eid, identity.eid);
    EXPECT_EQ(decoded->gid, identity.gid);
}
