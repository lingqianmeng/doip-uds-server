#include "doip_uds/vehicle_identification_server.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <system_error>
#include <thread>

#include "doip_uds/vehicle_identification_handler.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif

namespace doip_uds {
namespace {

constexpr char kIpv4LimitedBroadcastAddress[] = "255.255.255.255";
constexpr std::uint16_t kAnnouncementCount = 3;
constexpr auto kAnnouncementInterval = std::chrono::milliseconds(500);

std::chrono::milliseconds initial_announcement_delay() {
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<int> delay_ms(0, 500);
    return std::chrono::milliseconds(delay_ms(generator));
}

std::string vin_string(const EcuIdentity& identity) {
    return {identity.vin.begin(), identity.vin.end()};
}

bool error_is(const std::error_code& code, std::errc condition) {
    return code == std::make_error_condition(condition);
}

// Permission and reachability failures are specific to one destination. The
// listening socket can still receive and can still answer other peers.
// Examples: EPERM, EACCES, ENETUNREACH, EHOSTUNREACH, ECONNREFUSED, EMSGSIZE,
// ENOBUFS, and EAGAIN. Only errors that mean this socket can no longer send
// are fatal.
bool udp_send_failure_is_fatal(const std::system_error& error) {
    const std::error_code code = error.code();
    if (error_is(code, std::errc::bad_file_descriptor) ||
        error_is(code, std::errc::not_a_socket) ||
        error_is(code, std::errc::bad_address) ||
        error_is(code, std::errc::broken_pipe)) {
        return true;
    }
#ifdef _WIN32
    if (code.category() == std::system_category()) {
        switch (code.value()) {
        case WSAEBADF:
        case WSAENOTSOCK:
        case WSAEFAULT:
        case WSAESHUTDOWN:
            return true;
        default:
            break;
        }
    }
#endif
    return false;
}

}  // namespace

void send_startup_vehicle_announcements(UdpTransport& transport, const EcuIdentity& identity) {
    const auto announcement = make_vehicle_identification_response_datagram(identity);
    std::this_thread::sleep_for(initial_announcement_delay());

    for (std::uint16_t index = 0; index < kAnnouncementCount; ++index) {
        try {
            transport.send_to(kIpv4LimitedBroadcastAddress, kDoipUdpPort, announcement);
            std::cout << "[VAM startup] Sent " << (index + 1) << '/' << kAnnouncementCount
                      << " to " << kIpv4LimitedBroadcastAddress << ':' << kDoipUdpPort
                      << " VIN=" << vin_string(identity) << std::endl;
        } catch (const std::exception& error) {
            std::cout << "[VAM startup] Warning: announcement " << (index + 1) << '/'
                      << kAnnouncementCount << " failed: " << error.what() << std::endl;
        }
        // Keep the ISO 13400-2 spacing even when this announcement could not be sent.
        if (index + 1 < kAnnouncementCount) {
            std::this_thread::sleep_for(kAnnouncementInterval);
        }
    }
}

bool process_next_vehicle_identification_request(
    UdpTransport& transport,
    const EcuIdentity& identity,
    std::chrono::milliseconds receive_timeout) {
    const auto datagram = transport.receive_for(receive_timeout);
    if (!datagram) {
        return false;
    }

    const auto response =
        handle_vehicle_identification_datagram(identity, datagram->payload);
    if (!response) {
        return true;
    }

    try {
        transport.send_to(datagram->sender_address, datagram->sender_port, *response);
    } catch (const std::system_error& error) {
        if (udp_send_failure_is_fatal(error)) {
            throw;
        }
        std::cout << "[VAM response] Warning: response to " << datagram->sender_address
                  << ':' << datagram->sender_port << " failed: " << error.what() << std::endl;
        return true;
    }

    std::cout << "[VAM response] Sent to " << datagram->sender_address << ':'
              << datagram->sender_port << " VIN=" << vin_string(identity) << std::endl;
    return true;
}

}  // namespace doip_uds
