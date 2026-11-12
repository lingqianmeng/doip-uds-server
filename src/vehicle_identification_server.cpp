#include "doip_uds/vehicle_identification_server.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "doip_uds/vehicle_identification_handler.hpp"

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

}  // namespace

void send_startup_vehicle_announcements(UdpListener& listener, const EcuIdentity& identity) {
    const auto announcement = make_vehicle_identification_response_datagram(identity);
    std::this_thread::sleep_for(initial_announcement_delay());

    for (std::uint16_t index = 0; index < kAnnouncementCount; ++index) {
        listener.send_to(kIpv4LimitedBroadcastAddress, kDoipUdpPort, announcement);
        std::cout << "[VAM startup] Sent " << (index + 1) << '/' << kAnnouncementCount << " to "
                  << kIpv4LimitedBroadcastAddress << ':' << kDoipUdpPort
                  << " VIN=" << vin_string(identity) << '\n';
        if (index + 1 < kAnnouncementCount) {
            std::this_thread::sleep_for(kAnnouncementInterval);
        }
    }
}

bool process_next_vehicle_identification_request(
    UdpListener& listener,
    const EcuIdentity& identity,
    std::chrono::milliseconds receive_timeout) {
    const auto datagram = listener.receive_for(receive_timeout);
    if (!datagram) {
        return false;
    }

    const auto response =
        handle_vehicle_identification_datagram(identity, datagram->payload);
    if (response) {
        listener.send_to(datagram->sender_address, datagram->sender_port, *response);
        std::cout << "[VAM response] Sent to " << datagram->sender_address << ':'
                  << datagram->sender_port << " VIN=" << vin_string(identity) << '\n';
    }
    return true;
}

}  // namespace doip_uds
