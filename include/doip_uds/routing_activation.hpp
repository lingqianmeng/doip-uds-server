#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace doip_uds {

inline constexpr std::uint16_t kDoipPayloadTypeRoutingActivationRequest = 0x0005;
inline constexpr std::uint16_t kDoipPayloadTypeRoutingActivationResponse = 0x0006;

inline constexpr std::uint8_t kDoipDefaultRoutingActivationType = 0x00;
inline constexpr std::uint16_t kDoipFirstTestEquipmentLogicalAddress = 0x0E00;
inline constexpr std::uint16_t kDoipLastTestEquipmentLogicalAddress = 0x0FFF;

inline constexpr std::size_t kDoipRoutingActivationRequestPayloadSize = 7;
inline constexpr std::size_t kDoipRoutingActivationResponsePayloadSize = 9;
inline constexpr std::size_t kDoipRoutingActivationOemDataSize = 4;

enum class RoutingActivationResponseCode : std::uint8_t {
    unknown_source_address = 0x00,
    different_source_address_on_activated_socket = 0x02,
    source_address_active_on_another_socket = 0x03,
    unsupported_activation_type = 0x06,
    routing_successfully_activated = 0x10,
};

struct RoutingActivationRequest {
    std::uint16_t tester_logical_address{};
    std::uint8_t activation_type{};
    std::optional<std::array<std::uint8_t, kDoipRoutingActivationOemDataSize>> oem_data;
};

struct RoutingActivationResponse {
    std::uint16_t tester_logical_address{};
    std::uint16_t entity_logical_address{};
    RoutingActivationResponseCode response_code{
        RoutingActivationResponseCode::unknown_source_address};
    std::optional<std::array<std::uint8_t, kDoipRoutingActivationOemDataSize>> oem_data;
};

/// Encodes an ISO 13400-2:2012 routing activation request payload.
std::vector<std::uint8_t> encode_routing_activation_request(
    const RoutingActivationRequest& request);

/// Decodes the base request payload and its optional four-byte OEM extension.
std::optional<RoutingActivationRequest> decode_routing_activation_request(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept;

/// Encodes a routing activation response payload. ISO-reserved bytes are zeroed.
std::vector<std::uint8_t> encode_routing_activation_response(
    const RoutingActivationResponse& response);

/// Decodes the base response payload and its optional four-byte OEM extension.
std::optional<RoutingActivationResponse> decode_routing_activation_response(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept;

class RoutingActivationSession {
public:
    /// Evaluates a request and records the tester address only on success.
    RoutingActivationResponse handle_request(const RoutingActivationRequest& request,
                                             std::uint16_t entity_logical_address,
                                             bool active_on_another_socket);

    std::optional<std::uint16_t> active_tester_logical_address() const noexcept;

private:
    std::optional<std::uint16_t> active_tester_logical_address_;
};

}  // namespace doip_uds
