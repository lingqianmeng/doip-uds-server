#include "doip_uds/uds_dispatcher.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace doip_uds {
namespace {

constexpr std::uint8_t kUdsNegativeResponseServiceId = 0x7F;
constexpr std::uint8_t kSuppressPositiveResponseMask = 0x80;
constexpr std::uint8_t kUdsPositiveResponseReadDataByIdentifier = 0x62;
constexpr std::uint8_t kUdsPositiveResponseWriteDataByIdentifier = 0x6E;

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U) |
        static_cast<std::uint16_t>(bytes[offset + 1]));
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

std::vector<std::uint8_t> negative_response(std::uint8_t service_id,
                                            std::uint8_t response_code) {
    return {kUdsNegativeResponseServiceId, service_id, response_code};
}

std::optional<std::vector<std::uint8_t>> positive_response(
    bool suppress,
    std::vector<std::uint8_t> response) {
    if (suppress) {
        return std::nullopt;
    }
    return response;
}

std::uint8_t session_control_response_subfunction(std::uint8_t subfunction) {
    return static_cast<std::uint8_t>(subfunction & ~kSuppressPositiveResponseMask);
}

}  // namespace

UdsDataIdentifierStore make_uds_data_identifier_store(const EcuIdentity& identity) {
    UdsDataIdentifierStore data_identifiers;
    data_identifiers.emplace(
        kUdsVinDataIdentifier,
        UdsDataIdentifier{
            std::vector<std::uint8_t>(identity.vin.begin(), identity.vin.end()), true, false});
    for (const auto& did : identity.dids) {
        if (did.identifier == kUdsVinDataIdentifier ||
            !data_identifiers
                 .emplace(did.identifier,
                          UdsDataIdentifier{did.initial_value, did.readable, did.writable})
                 .second) {
            throw std::invalid_argument("ECU identity contains a duplicate or reserved DID");
        }
    }
    return data_identifiers;
}

UdsDispatchResult dispatch_uds_request(const std::vector<std::uint8_t>& request,
                                       UdsConnectionState& state,
                                       UdsDataIdentifierStore& data_identifiers) {
    if (request.empty()) {
        throw std::invalid_argument("UDS request must contain a service identifier");
    }

    const auto service_id = request.front();
    switch (service_id) {
        case kUdsServiceSessionControl: {
            if (request.size() != 2) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            const auto requested_session = static_cast<std::uint8_t>(request[1] & 0x7FU);
            UdsDiagnosticSession session;
            if (requested_session == static_cast<std::uint8_t>(
                                         UdsDiagnosticSession::default_session)) {
                session = UdsDiagnosticSession::default_session;
            } else if (requested_session == static_cast<std::uint8_t>(
                                                UdsDiagnosticSession::extended_session)) {
                session = UdsDiagnosticSession::extended_session;
            } else {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeSubfunctionNotSupported),
                        std::nullopt};
            }

            state.session = session;
            const auto response_subfunction =
                session_control_response_subfunction(request[1]);
            return {positive_response(
                        (request[1] & kSuppressPositiveResponseMask) != 0,
                        {0x50,
                         response_subfunction,
                         static_cast<std::uint8_t>(
                             (kUdsP2ServerMaximumMilliseconds >> 8U) & 0xFFU),
                         static_cast<std::uint8_t>(kUdsP2ServerMaximumMilliseconds & 0xFFU),
                         static_cast<std::uint8_t>(
                             (kUdsP2StarServerMaximum10Milliseconds >> 8U) & 0xFFU),
                         static_cast<std::uint8_t>(
                             kUdsP2StarServerMaximum10Milliseconds & 0xFFU)}),
                    std::nullopt};
        }

        case kUdsServiceTesterPresent: {
            if (request.size() != 2) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            const bool suppress = (request[1] & kSuppressPositiveResponseMask) != 0;
            const auto subfunction = static_cast<std::uint8_t>(request[1] & 0x7FU);
            if (subfunction != 0x00) {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeSubfunctionNotSupported),
                        std::nullopt};
            }
            return {positive_response(suppress, {0x7E, subfunction}), std::nullopt};
        }

        case kUdsServiceEcuReset: {
            if (request.size() != 2) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            const bool suppress = (request[1] & kSuppressPositiveResponseMask) != 0;
            const auto reset_type = static_cast<std::uint8_t>(request[1] & 0x7FU);
            if (reset_type != static_cast<std::uint8_t>(UdsEcuResetType::hard_reset)) {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeSubfunctionNotSupported),
                        std::nullopt};
            }

            state.session = UdsDiagnosticSession::default_session;
            return {positive_response(suppress, {0x51, reset_type}),
                    UdsEcuResetType::hard_reset};
        }

        case kUdsServiceReadDataByIdentifier: {
            if (request.size() < 3 || (request.size() - 1) % 2 != 0) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            std::vector<std::uint8_t> response{kUdsPositiveResponseReadDataByIdentifier};
            for (std::size_t offset = 1; offset < request.size(); offset += 2) {
                const auto identifier = read_u16(request, offset);
                const auto did = data_identifiers.find(identifier);
                if (did == data_identifiers.end() || !did->second.readable) {
                    return {negative_response(
                                service_id, kUdsNegativeResponseCodeRequestOutOfRange),
                            std::nullopt};
                }
                append_u16(response, identifier);
                response.insert(response.end(), did->second.value.begin(), did->second.value.end());
            }
            return {std::move(response), std::nullopt};
        }

        case kUdsServiceWriteDataByIdentifier: {
            if (request.size() < 4) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }
            if (state.session != UdsDiagnosticSession::extended_session) {
                return {negative_response(
                            service_id,
                            kUdsNegativeResponseCodeServiceNotSupportedInActiveSession),
                        std::nullopt};
            }

            const auto identifier = read_u16(request, 1);
            const auto did = data_identifiers.find(identifier);
            if (did == data_identifiers.end() || !did->second.writable) {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeRequestOutOfRange),
                        std::nullopt};
            }
            if (request.size() != did->second.value.size() + 3) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            did->second.value.assign(request.begin() + 3, request.end());
            return {std::vector<std::uint8_t>{
                         kUdsPositiveResponseWriteDataByIdentifier,
                         static_cast<std::uint8_t>((identifier >> 8U) & 0xFFU),
                         static_cast<std::uint8_t>(identifier & 0xFFU)},
                    std::nullopt};
        }

        default:
            return {negative_response(service_id, kUdsNegativeResponseCodeServiceNotSupported),
                    std::nullopt};
    }
}

}  // namespace doip_uds
