#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/doip_header.hpp"
#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/udp_transport.hpp"
#include "doip_uds/vehicle_identification.hpp"
#include "doip_uds/vehicle_identification_server.hpp"

namespace {

const doip_uds::EcuIdentity kIdentity{
    {'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'},
    0x1001,
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    {},
    {}};

class CapturedStdout {
public:
    CapturedStdout() : previous_(std::cout.rdbuf(buffer_.rdbuf())) {}

    ~CapturedStdout() {
        std::cout.rdbuf(previous_);
    }

    CapturedStdout(const CapturedStdout&) = delete;
    CapturedStdout& operator=(const CapturedStdout&) = delete;

    std::string str() const {
        return buffer_.str();
    }

private:
    std::stringstream buffer_;
    std::streambuf* previous_;
};

class FakeUdpTransport : public doip_uds::UdpTransport {
public:
    std::optional<doip_uds::UdpDatagram> receive_for(std::chrono::milliseconds) override {
        if (receive_error) {
            std::rethrow_exception(receive_error);
        }
        auto delivered = incoming;
        incoming.reset();
        return delivered;
    }

    void send_to(const std::string& address,
                 std::uint16_t port,
                 const std::vector<std::uint8_t>& payload) override {
        destinations.emplace_back(address, port);
        sent_bytes += payload.size();
        ++send_attempts;
        if (send_error) {
            std::rethrow_exception(send_error);
        }
    }

    std::optional<doip_uds::UdpDatagram> incoming;
    std::exception_ptr receive_error;
    std::exception_ptr send_error;
    int send_attempts = 0;
    std::size_t sent_bytes = 0;
    std::vector<std::pair<std::string, std::uint16_t>> destinations;
};

std::vector<std::uint8_t> broadcast_vehicle_identification_request() {
    const doip_uds::DoipHeader header{
        doip_uds::kDoipDefaultProtocolVersionForVehicleIdentificationRequest,
        doip_uds::kDoipPayloadTypeVehicleIdentificationRequest,
        0};
    const auto encoded = doip_uds::encode_doip_header(header);
    return {encoded.begin(), encoded.end()};
}

std::system_error send_failure(std::errc code) {
    return std::system_error(std::make_error_code(code), "sendto");
}

}  // namespace

TEST(VehicleIdentificationServer, FailedStartupSendLogsWarningAndContinues) {
    FakeUdpTransport transport;
    const auto failure = send_failure(std::errc::operation_not_permitted);
    transport.send_error = std::make_exception_ptr(failure);

    CapturedStdout captured;
    const auto started = std::chrono::steady_clock::now();
    EXPECT_NO_THROW(doip_uds::send_startup_vehicle_announcements(transport, kIdentity));
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - started)
                                .count();
    const std::string log = captured.str();

    EXPECT_EQ(transport.send_attempts, 3);
    EXPECT_GT(transport.sent_bytes, 0U);
    ASSERT_EQ(transport.destinations.size(), 3U);
    for (const auto& destination : transport.destinations) {
        EXPECT_EQ(destination.first, "255.255.255.255");
        EXPECT_EQ(destination.second, doip_uds::kDoipUdpPort);
    }

    const std::string reason = failure.what();
    EXPECT_NE(log.find("[VAM startup] Warning: announcement 1/3 failed: " + reason),
              std::string::npos);
    EXPECT_NE(log.find("[VAM startup] Warning: announcement 2/3 failed: " + reason),
              std::string::npos);
    EXPECT_NE(log.find("[VAM startup] Warning: announcement 3/3 failed: " + reason),
              std::string::npos);
    EXPECT_EQ(log.find("[VAM startup] Sent "), std::string::npos);
    // Two 500 ms gaps are kept even though every send failed. The random
    // initial delay is at most 500 ms, so this is longer than that delay alone.
    EXPECT_GE(elapsed_ms, 1000);
    EXPECT_LT(elapsed_ms, 10000);
}

TEST(VehicleIdentificationServer, FailedIdentificationResponseSendLogsWarning) {
    FakeUdpTransport transport;
    const auto failure = send_failure(std::errc::operation_not_permitted);
    transport.send_error = std::make_exception_ptr(failure);
    transport.incoming = doip_uds::UdpDatagram{
        broadcast_vehicle_identification_request(), "192.0.2.15", 34567};

    CapturedStdout captured;
    bool handled = false;
    EXPECT_NO_THROW(handled = doip_uds::process_next_vehicle_identification_request(
                        transport, kIdentity, std::chrono::milliseconds(1)));

    EXPECT_TRUE(handled);
    EXPECT_EQ(transport.send_attempts, 1);
    const std::string log = captured.str();
    EXPECT_NE(log.find("[VAM response] Warning: response to 192.0.2.15:34567 failed: " +
                       std::string(failure.what())),
              std::string::npos);
    EXPECT_EQ(log.find("[VAM response] Sent "), std::string::npos);
}

TEST(VehicleIdentificationServer, BrokenSocketResponseSendStaysFatal) {
    FakeUdpTransport transport;
    transport.send_error = std::make_exception_ptr(
        send_failure(std::errc::bad_file_descriptor));
    transport.incoming = doip_uds::UdpDatagram{
        broadcast_vehicle_identification_request(), "192.0.2.15", 34567};

    CapturedStdout captured;
    EXPECT_THROW(doip_uds::process_next_vehicle_identification_request(
                     transport, kIdentity, std::chrono::milliseconds(1)),
                 std::system_error);
    EXPECT_EQ(captured.str().find("Warning"), std::string::npos);
    EXPECT_EQ(transport.send_attempts, 1);
}

TEST(VehicleIdentificationServer, ReceiveFailureStaysFatal) {
    FakeUdpTransport transport;
    transport.receive_error = std::make_exception_ptr(
        std::system_error(std::make_error_code(std::errc::bad_file_descriptor), "recvfrom"));

    EXPECT_THROW(doip_uds::process_next_vehicle_identification_request(
                     transport, kIdentity, std::chrono::milliseconds(1)),
                 std::system_error);
    EXPECT_EQ(transport.send_attempts, 0);
}
