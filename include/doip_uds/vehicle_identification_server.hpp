#pragma once

#include <chrono>

#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/udp_transport.hpp"

namespace doip_uds {

/// Sends the three startup vehicle-identification announcements to the IPv4
/// limited-broadcast address, using DoIP announcement timing.
/// Each send is best effort. A failure is logged and the remaining
/// announcements are still attempted after the normal interval. A failed send
/// does not throw.
void send_startup_vehicle_announcements(UdpTransport& transport, const EcuIdentity& identity);

/// Receives at most one datagram, handles a matching vehicle-identification
/// request, and sends its response to the datagram's sender.
/// Returns true if a datagram was received, including malformed requests.
/// A failed response send is logged and ignored, unless the error means the
/// UDP socket itself is unusable, in which case that exception propagates.
/// Receive failures also propagate.
bool process_next_vehicle_identification_request(
    UdpTransport& transport,
    const EcuIdentity& identity,
    std::chrono::milliseconds receive_timeout);

}  // namespace doip_uds
