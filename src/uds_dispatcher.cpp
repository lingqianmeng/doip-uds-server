#include "doip_uds/uds_dispatcher.hpp"

#include <stdexcept>

namespace doip_uds {
namespace {

constexpr std::uint8_t kUdsNegativeResponseServiceId = 0x7F;
constexpr std::uint8_t kSuppressPositiveResponseMask = 0x80;

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

UdsDispatchResult dispatch_uds_request(const std::vector<std::uint8_t>& request,
                                       UdsConnectionState& state) {
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

        default:
            return {negative_response(service_id, kUdsNegativeResponseCodeServiceNotSupported),
                    std::nullopt};
    }
}

}  // namespace doip_uds
