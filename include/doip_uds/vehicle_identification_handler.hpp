#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "doip_uds/ecu_identity.hpp"

namespace doip_uds {

/// Builds the complete DoIP vehicle-identification response/announcement
/// datagram for the supplied ECU identity.
std::vector<std::uint8_t> make_vehicle_identification_response_datagram(
    const EcuIdentity& identity);

/// Returns a complete DoIP identification response for a valid matching request,
/// or nullopt when the datagram is malformed, unsupported, or targets another ECU.
std::optional<std::vector<std::uint8_t>> handle_vehicle_identification_datagram(
    const EcuIdentity& identity,
    const std::vector<std::uint8_t>& datagram);

}  // namespace doip_uds
