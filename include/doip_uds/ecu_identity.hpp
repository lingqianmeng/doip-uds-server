#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

#include "doip_uds/vehicle_identification.hpp"

namespace doip_uds {

struct EcuIdentity {
    std::array<std::uint8_t, kDoipVinSize> vin{};
    std::uint16_t logical_address{};
    std::array<std::uint8_t, kDoipEntityIdSize> eid{};
    std::array<std::uint8_t, kDoipEntityIdSize> gid{};
};

/// Loads and validates the required ECU identity fields from a JSON file.
/// Throws std::runtime_error with the file or field that caused the failure.
EcuIdentity load_ecu_identity(const std::filesystem::path& config_path);

}  // namespace doip_uds
