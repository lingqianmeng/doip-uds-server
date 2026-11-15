#include "doip_uds/routing_activation.hpp"

#include <algorithm>

namespace doip_uds {
namespace {

constexpr std::size_t kRequestOemDataOffset = kDoipRoutingActivationRequestPayloadSize;
constexpr std::size_t kResponseOemDataOffset = kDoipRoutingActivationResponsePayloadSize;
constexpr std::size_t kExtendedPayloadSize =
    kDoipRoutingActivationRequestPayloadSize + kDoipRoutingActivationOemDataSize;
constexpr std::size_t kExtendedResponsePayloadSize =
    kDoipRoutingActivationResponsePayloadSize + kDoipRoutingActivationOemDataSize;

bool valid_request_size(std::size_t payload_size) noexcept {
    return payload_size == kDoipRoutingActivationRequestPayloadSize ||
           payload_size == kExtendedPayloadSize;
}

bool valid_response_size(std::size_t payload_size) noexcept {
    return payload_size == kDoipRoutingActivationResponsePayloadSize ||
           payload_size == kExtendedResponsePayloadSize;
}

std::uint16_t read_u16(const std::uint8_t* input) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(input[0]) << 8U) | static_cast<std::uint16_t>(input[1]));
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

}  // namespace

std::vector<std::uint8_t> encode_routing_activation_request(
    const RoutingActivationRequest& request) {
    std::vector<std::uint8_t> payload;
    payload.reserve(kDoipRoutingActivationRequestPayloadSize +
                    (request.oem_data ? kDoipRoutingActivationOemDataSize : 0));
    append_u16(payload, request.tester_logical_address);
    payload.push_back(request.activation_type);
    payload.insert(payload.end(), 4, 0);
    if (request.oem_data) {
        payload.insert(payload.end(), request.oem_data->begin(), request.oem_data->end());
    }
    return payload;
}

std::optional<RoutingActivationRequest> decode_routing_activation_request(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    if (payload == nullptr || !valid_request_size(payload_size)) {
        return std::nullopt;
    }

    RoutingActivationRequest request;
    request.tester_logical_address = read_u16(payload);
    request.activation_type = payload[2];
    if (payload_size == kExtendedPayloadSize) {
        std::array<std::uint8_t, kDoipRoutingActivationOemDataSize> oem_data{};
        std::copy_n(payload + kRequestOemDataOffset, oem_data.size(), oem_data.begin());
        request.oem_data = oem_data;
    }
    return request;
}

std::vector<std::uint8_t> encode_routing_activation_response(
    const RoutingActivationResponse& response) {
    std::vector<std::uint8_t> payload;
    payload.reserve(kDoipRoutingActivationResponsePayloadSize +
                    (response.oem_data ? kDoipRoutingActivationOemDataSize : 0));
    append_u16(payload, response.tester_logical_address);
    append_u16(payload, response.entity_logical_address);
    payload.push_back(static_cast<std::uint8_t>(response.response_code));
    payload.insert(payload.end(), 4, 0);
    if (response.oem_data) {
        payload.insert(payload.end(), response.oem_data->begin(), response.oem_data->end());
    }
    return payload;
}

std::optional<RoutingActivationResponse> decode_routing_activation_response(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    if (payload == nullptr || !valid_response_size(payload_size)) {
        return std::nullopt;
    }

    RoutingActivationResponse response;
    response.tester_logical_address = read_u16(payload);
    response.entity_logical_address = read_u16(payload + 2);
    response.response_code = static_cast<RoutingActivationResponseCode>(payload[4]);
    if (payload_size == kExtendedResponsePayloadSize) {
        std::array<std::uint8_t, kDoipRoutingActivationOemDataSize> oem_data{};
        std::copy_n(payload + kResponseOemDataOffset, oem_data.size(), oem_data.begin());
        response.oem_data = oem_data;
    }
    return response;
}

RoutingActivationResponse RoutingActivationSession::handle_request(
    const RoutingActivationRequest& request,
    std::uint16_t entity_logical_address,
    bool active_on_another_socket) {
    auto response_code = RoutingActivationResponseCode::routing_successfully_activated;

    if (request.tester_logical_address < kDoipFirstTestEquipmentLogicalAddress ||
        request.tester_logical_address > kDoipLastTestEquipmentLogicalAddress) {
        response_code = RoutingActivationResponseCode::unknown_source_address;
    } else if (request.activation_type != kDoipDefaultRoutingActivationType) {
        response_code = RoutingActivationResponseCode::unsupported_activation_type;
    } else if (active_tester_logical_address_ &&
               *active_tester_logical_address_ != request.tester_logical_address) {
        response_code =
            RoutingActivationResponseCode::different_source_address_on_activated_socket;
    } else if (active_on_another_socket) {
        response_code = RoutingActivationResponseCode::source_address_active_on_another_socket;
    } else {
        active_tester_logical_address_ = request.tester_logical_address;
    }

    return {request.tester_logical_address, entity_logical_address, response_code, std::nullopt};
}

std::optional<std::uint16_t> RoutingActivationSession::active_tester_logical_address() const noexcept {
    return active_tester_logical_address_;
}

}  // namespace doip_uds
