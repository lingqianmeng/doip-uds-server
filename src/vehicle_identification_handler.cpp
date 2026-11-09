#include "doip_uds/vehicle_identification_handler.hpp"

#include <type_traits>

#include "doip_uds/doip_header.hpp"
#include "doip_uds/vehicle_identification.hpp"

namespace doip_uds {

std::optional<std::vector<std::uint8_t>> handle_vehicle_identification_datagram(
    const EcuIdentity& identity,
    const std::vector<std::uint8_t>& datagram) {
    const auto header = decode_doip_header(datagram.data(), datagram.size());
    if (!header ||
        header->protocol_version != kDoipDefaultProtocolVersionForVehicleIdentificationRequest ||
        datagram.size() - kDoipGenericHeaderSize != header->payload_length) {
        return std::nullopt;
    }

    const auto* payload = datagram.data() + kDoipGenericHeaderSize;
    const auto payload_size = datagram.size() - kDoipGenericHeaderSize;
    const auto request =
        decode_vehicle_identification_request(header->payload_type, payload, payload_size);
    if (!request) {
        return std::nullopt;
    }

    const bool matches_identity = std::visit(
        [&identity](const auto& typed_request) {
            using Request = std::decay_t<decltype(typed_request)>;
            if constexpr (std::is_same_v<Request, VehicleIdentificationRequest>) {
                return true;
            } else if constexpr (std::is_same_v<Request, VehicleIdentificationRequestByVin>) {
                return typed_request.vin == identity.vin;
            } else {
                return typed_request.eid == identity.eid;
            }
        },
        *request);
    if (!matches_identity) {
        return std::nullopt;
    }

    const VehicleIdentificationResponse response{
        identity.vin,
        identity.logical_address,
        identity.eid,
        identity.gid,
        0x00,
        0x00};
    const auto response_payload = encode_vehicle_identification_response(response);
    const DoipHeader response_header{
        kDoipProtocolVersion,
        kDoipPayloadTypeVehicleIdentificationResponse,
        static_cast<std::uint32_t>(response_payload.size())};
    const auto encoded_header = encode_doip_header(response_header);

    std::vector<std::uint8_t> response_datagram;
    response_datagram.reserve(encoded_header.size() + response_payload.size());
    response_datagram.insert(response_datagram.end(), encoded_header.begin(), encoded_header.end());
    response_datagram.insert(
        response_datagram.end(), response_payload.begin(), response_payload.end());
    return response_datagram;
}

}  // namespace doip_uds
