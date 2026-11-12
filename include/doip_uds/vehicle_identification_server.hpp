#pragma once

#include <chrono>

#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/udp_transport.hpp"

namespace doip_uds {

/// Sends the three startup vehicle-identification announcements to the IPv4
/// limited-broadcast address, using DoIP announcement timing.
void send_startup_vehicle_announcements(UdpListener& listener, const EcuIdentity& identity);

/// Receives at most one datagram, handles a matching vehicle-identification
/// request, and sends its response to the datagram's sender.
/// Returns true if a datagram was received, including malformed requests.
bool process_next_vehicle_identification_request(
    UdpListener& listener,
    const EcuIdentity& identity,
    std::chrono::milliseconds receive_timeout);

}  // namespace doip_uds
