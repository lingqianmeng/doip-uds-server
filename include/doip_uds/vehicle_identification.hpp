#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace doip_uds {

inline constexpr std::uint16_t kDoipPayloadTypeVehicleIdentificationRequest = 0x0001;
inline constexpr std::uint16_t kDoipPayloadTypeVehicleIdentificationRequestByEid = 0x0002;
inline constexpr std::uint16_t kDoipPayloadTypeVehicleIdentificationRequestByVin = 0x0003;
inline constexpr std::uint16_t kDoipPayloadTypeVehicleIdentificationResponse = 0x0004;

inline constexpr std::size_t kDoipVinSize = 17;
inline constexpr std::size_t kDoipEntityIdSize = 6;
inline constexpr std::size_t kDoipVehicleIdentificationResponsePayloadSize = 33;

struct VehicleIdentificationRequest {};

struct VehicleIdentificationRequestByVin {
    std::array<std::uint8_t, kDoipVinSize> vin{};
};

struct VehicleIdentificationRequestByEid {
    std::array<std::uint8_t, kDoipEntityIdSize> eid{};
};

using VehicleIdentificationRequestMessage =
    std::variant<VehicleIdentificationRequest,
                 VehicleIdentificationRequestByVin,
                 VehicleIdentificationRequestByEid>;

struct EncodedVehicleIdentificationRequest {
    std::uint16_t payload_type{};
    std::vector<std::uint8_t> payload;
};

struct VehicleIdentificationResponse {
    std::array<std::uint8_t, kDoipVinSize> vin{};
    std::uint16_t logical_address{};
    std::array<std::uint8_t, kDoipEntityIdSize> eid{};
    std::array<std::uint8_t, kDoipEntityIdSize> gid{};
    std::uint8_t further_action{};
    std::uint8_t synchronization_status{};
};

/// Encodes a vehicle-identification request as its payload type and payload.
EncodedVehicleIdentificationRequest encode_vehicle_identification_request(
    const VehicleIdentificationRequestMessage& request);

/// Decodes request payloads for the broadcast, VIN-targeted, and EID-targeted
/// vehicle-identification request types. The payload size must match exactly.
std::optional<VehicleIdentificationRequestMessage> decode_vehicle_identification_request(
    std::uint16_t payload_type,
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept;

/// Encodes the 33-byte vehicle-identification response payload in network byte order.
std::array<std::uint8_t, kDoipVehicleIdentificationResponsePayloadSize>
encode_vehicle_identification_response(const VehicleIdentificationResponse& response) noexcept;

/// Decodes an exact-size vehicle-identification response payload.
std::optional<VehicleIdentificationResponse> decode_vehicle_identification_response(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept;

}  // namespace doip_uds
