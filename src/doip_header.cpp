#include "doip_uds/doip_header.hpp"

namespace doip_uds {

std::array<std::uint8_t, kDoipGenericHeaderSize> encode_doip_header(
    const DoipHeader& header) noexcept {
    return {
        header.protocol_version,
        static_cast<std::uint8_t>(~header.protocol_version),
        static_cast<std::uint8_t>((header.payload_type >> 8U) & 0xFFU),
        static_cast<std::uint8_t>(header.payload_type & 0xFFU),
        static_cast<std::uint8_t>((header.payload_length >> 24U) & 0xFFU),
        static_cast<std::uint8_t>((header.payload_length >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((header.payload_length >> 8U) & 0xFFU),
        static_cast<std::uint8_t>(header.payload_length & 0xFFU),
    };
}

std::optional<DoipHeader> decode_doip_header(const std::uint8_t* input,
                                             std::size_t input_size) noexcept {
    if (input == nullptr || input_size < kDoipGenericHeaderSize) {
        return std::nullopt;
    }

    const auto protocol_version = input[0];
    if (input[1] != static_cast<std::uint8_t>(~protocol_version)) {
        return std::nullopt;
    }

    const auto payload_type = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(input[2]) << 8U) | static_cast<std::uint16_t>(input[3]));
    const auto payload_length =
        (static_cast<std::uint32_t>(input[4]) << 24U) |
        (static_cast<std::uint32_t>(input[5]) << 16U) |
        (static_cast<std::uint32_t>(input[6]) << 8U) |
        static_cast<std::uint32_t>(input[7]);

    return DoipHeader{protocol_version, payload_type, payload_length};
}

}  // namespace doip_uds
