#include "doip_uds/vehicle_identification.hpp"

#include <algorithm>
#include <type_traits>

namespace doip_uds {
namespace {

template <std::size_t Size>
void copy_to_payload(const std::array<std::uint8_t, Size>& source,
                     std::uint8_t* destination) noexcept {
    std::copy(source.begin(), source.end(), destination);
}

template <std::size_t Size>
void copy_from_payload(const std::uint8_t* source,
                       std::array<std::uint8_t, Size>& destination) noexcept {
    std::copy_n(source, Size, destination.begin());
}

}  // namespace

EncodedVehicleIdentificationRequest encode_vehicle_identification_request(
    const VehicleIdentificationRequestMessage& request) {
    return std::visit(
        [](const auto& typed_request) -> EncodedVehicleIdentificationRequest {
            using Request = std::decay_t<decltype(typed_request)>;
            if constexpr (std::is_same_v<Request, VehicleIdentificationRequest>) {
                return {kDoipPayloadTypeVehicleIdentificationRequest, {}};
            } else if constexpr (std::is_same_v<Request, VehicleIdentificationRequestByVin>) {
                return {kDoipPayloadTypeVehicleIdentificationRequestByVin,
                        {typed_request.vin.begin(), typed_request.vin.end()}};
            } else {
                return {kDoipPayloadTypeVehicleIdentificationRequestByEid,
                        {typed_request.eid.begin(), typed_request.eid.end()}};
            }
        },
        request);
}

std::optional<VehicleIdentificationRequestMessage> decode_vehicle_identification_request(
    std::uint16_t payload_type,
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    switch (payload_type) {
        case kDoipPayloadTypeVehicleIdentificationRequest:
            if (payload_size == 0) {
                return VehicleIdentificationRequest{};
            }
            return std::nullopt;

        case kDoipPayloadTypeVehicleIdentificationRequestByVin: {
            if (payload == nullptr || payload_size != kDoipVinSize) {
                return std::nullopt;
            }
            VehicleIdentificationRequestByVin request;
            copy_from_payload(payload, request.vin);
            return request;
        }

        case kDoipPayloadTypeVehicleIdentificationRequestByEid: {
            if (payload == nullptr || payload_size != kDoipEntityIdSize) {
                return std::nullopt;
            }
            VehicleIdentificationRequestByEid request;
            copy_from_payload(payload, request.eid);
            return request;
        }

        default:
            return std::nullopt;
    }
}

std::array<std::uint8_t, kDoipVehicleIdentificationResponsePayloadSize>
encode_vehicle_identification_response(const VehicleIdentificationResponse& response) noexcept {
    std::array<std::uint8_t, kDoipVehicleIdentificationResponsePayloadSize> payload{};
    auto* output = payload.data();

    copy_to_payload(response.vin, output);
    output += kDoipVinSize;
    *output++ = static_cast<std::uint8_t>((response.logical_address >> 8U) & 0xFFU);
    *output++ = static_cast<std::uint8_t>(response.logical_address & 0xFFU);
    copy_to_payload(response.eid, output);
    output += kDoipEntityIdSize;
    copy_to_payload(response.gid, output);
    output += kDoipEntityIdSize;
    *output++ = response.further_action;
    *output = response.synchronization_status;

    return payload;
}

std::optional<VehicleIdentificationResponse> decode_vehicle_identification_response(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    if (payload == nullptr || payload_size != kDoipVehicleIdentificationResponsePayloadSize) {
        return std::nullopt;
    }

    VehicleIdentificationResponse response;
    auto* input = payload;

    copy_from_payload(input, response.vin);
    input += kDoipVinSize;
    response.logical_address =
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(input[0]) << 8U) |
                                   static_cast<std::uint16_t>(input[1]));
    input += 2;
    copy_from_payload(input, response.eid);
    input += kDoipEntityIdSize;
    copy_from_payload(input, response.gid);
    input += kDoipEntityIdSize;
    response.further_action = *input++;
    response.synchronization_status = *input;

    return response;
}

}  // namespace doip_uds
