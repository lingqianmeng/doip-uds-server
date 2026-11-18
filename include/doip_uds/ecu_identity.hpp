#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "doip_uds/dtc.hpp"
#include "doip_uds/vehicle_identification.hpp"

namespace doip_uds {

inline constexpr std::uint16_t kVinDataIdentifier = 0xF190;

struct EcuDidConfiguration {
    std::uint16_t identifier{};
    std::vector<std::uint8_t> initial_value;
    bool readable{};
    bool writable{};
};

struct EcuIdentity {
    std::array<std::uint8_t, kDoipVinSize> vin{};
    std::uint16_t logical_address{};
    std::array<std::uint8_t, kDoipEntityIdSize> eid{};
    std::array<std::uint8_t, kDoipEntityIdSize> gid{};
    std::vector<EcuDidConfiguration> dids;
    std::vector<EcuDtcConfiguration> dtcs;
};

/// Loads and validates the required ECU identity fields from a JSON file.
/// Throws std::runtime_error with the file or field that caused the failure.
EcuIdentity load_ecu_identity(const std::filesystem::path& config_path);

}  // namespace doip_uds
