#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace doip_uds {

inline constexpr std::size_t kDoipGenericHeaderSize = 8;
inline constexpr std::uint8_t kDoipProtocolVersion = 0x02;
inline constexpr std::uint8_t kDoipDefaultProtocolVersionForVehicleIdentificationRequest = 0xFF;

struct DoipHeader {
    std::uint8_t protocol_version{kDoipProtocolVersion};
    std::uint16_t payload_type{};
    std::uint32_t payload_length{};
};

/// Encodes a DoIP generic header in network byte order.
std::array<std::uint8_t, kDoipGenericHeaderSize> encode_doip_header(
    const DoipHeader& header) noexcept;

/// Decodes a DoIP generic header, or returns nullopt if it is truncated or its
/// protocol-version inverse byte is invalid.
std::optional<DoipHeader> decode_doip_header(const std::uint8_t* input,
                                             std::size_t input_size) noexcept;

}  // namespace doip_uds
