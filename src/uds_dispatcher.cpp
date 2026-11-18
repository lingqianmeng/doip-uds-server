#include "doip_uds/uds_dispatcher.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace doip_uds {
namespace {

constexpr std::uint8_t kUdsNegativeResponseServiceId = 0x7F;
constexpr std::uint8_t kSuppressPositiveResponseMask = 0x80;
constexpr std::uint8_t kUdsPositiveResponseSecurityAccess = 0x67;
constexpr std::uint8_t kUdsPositiveResponseReadDataByIdentifier = 0x62;
constexpr std::uint8_t kUdsPositiveResponseWriteDataByIdentifier = 0x6E;
constexpr std::uint8_t kUdsPositiveResponseReadDtcInformation = 0x59;
constexpr std::uint8_t kUdsPositiveResponseClearDiagnosticInformation = 0x54;
constexpr std::uint8_t kUdsReportNumberOfDtcByStatusMask = 0x01;
constexpr std::uint8_t kUdsReportDtcByStatusMask = 0x02;
constexpr std::uint8_t kUdsRequestSeedSubfunction = 0x01;
constexpr std::uint8_t kUdsSendKeySubfunction = 0x02;
constexpr std::uint16_t kSimulatorSecurityKeyMask = 0xA5C3;

void reset_security_state(UdsConnectionState& state) {
    state.pending_security_seed.reset();
    state.security_unlocked = false;
}

std::uint16_t simulator_security_key(std::uint16_t seed) {
    return static_cast<std::uint16_t>(seed ^ kSimulatorSecurityKeyMask);
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U) |
        static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t read_u24(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 2]);
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_dtc_and_status(std::vector<std::uint8_t>& response,
                           const UdsDtcRecord& dtc) {
    response.push_back(static_cast<std::uint8_t>((dtc.pseudo_dfcc >> 16U) & 0xFFU));
    response.push_back(static_cast<std::uint8_t>((dtc.pseudo_dfcc >> 8U) & 0xFFU));
    response.push_back(static_cast<std::uint8_t>(dtc.pseudo_dfcc & 0xFFU));
    response.push_back(dtc.status);
}

bool matches_status_mask(const UdsDtcRecord& dtc, std::uint8_t status_mask) {
    return (dtc.status & status_mask) != 0;
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

UdsDtcStore make_uds_dtc_store(const EcuIdentity& identity) {
    UdsDtcStore dtcs;
    dtcs.reserve(identity.dtcs.size());
    for (const auto& configured_dtc : identity.dtcs) {
        dtcs.push_back(
            {configured_dtc.symbolic_name, configured_dtc.pseudo_dfcc, configured_dtc.initial_status});
    }
    return dtcs;
}

UdsDispatchResult dispatch_uds_request(const std::vector<std::uint8_t>& request,
                                       UdsConnectionState& state,
                                       UdsDataIdentifierStore& data_identifiers,
                                       UdsDtcStore& dtcs) {
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

            if (state.session != session) {
                reset_security_state(state);
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

            reset_security_state(state);
            state.session = UdsDiagnosticSession::default_session;
            return {positive_response(suppress, {0x51, reset_type}),
                    UdsEcuResetType::hard_reset};
        }

        case kUdsServiceSecurityAccess: {
            if (request.size() < 2) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }
            if (state.session != UdsDiagnosticSession::extended_session) {
                return {negative_response(
                            service_id,
                            kUdsNegativeResponseCodeServiceNotSupportedInActiveSession),
                        std::nullopt};
            }

            const auto subfunction = request[1];
            if (subfunction == kUdsRequestSeedSubfunction) {
                if (request.size() != 2) {
                    return {negative_response(
                                service_id, kUdsNegativeResponseCodeIncorrectLength),
                            std::nullopt};
                }
                if (state.security_unlocked) {
                    return {std::vector<std::uint8_t>{
                                kUdsPositiveResponseSecurityAccess, subfunction, 0x00, 0x00},
                            std::nullopt};
                }

                const auto seed = state.next_security_seed;
                state.next_security_seed =
                    static_cast<std::uint16_t>(state.next_security_seed + 0x0101U);
                if (state.next_security_seed == 0) {
                    state.next_security_seed = 1;
                }
                state.pending_security_seed = seed;
                return {std::vector<std::uint8_t>{
                            kUdsPositiveResponseSecurityAccess,
                            subfunction,
                            static_cast<std::uint8_t>((seed >> 8U) & 0xFFU),
                            static_cast<std::uint8_t>(seed & 0xFFU)},
                        std::nullopt};
            }

            if (subfunction == kUdsSendKeySubfunction) {
                if (request.size() != 4) {
                    return {negative_response(
                                service_id, kUdsNegativeResponseCodeIncorrectLength),
                            std::nullopt};
                }
                if (!state.pending_security_seed) {
                    return {negative_response(
                                service_id, kUdsNegativeResponseCodeRequestSequenceError),
                            std::nullopt};
                }

                const auto supplied_key = read_u16(request, 2);
                const auto expected_key = simulator_security_key(*state.pending_security_seed);
                state.pending_security_seed.reset();
                if (supplied_key != expected_key) {
                    return {negative_response(service_id, kUdsNegativeResponseCodeInvalidKey),
                            std::nullopt};
                }

                state.security_unlocked = true;
                return {std::vector<std::uint8_t>{kUdsPositiveResponseSecurityAccess,
                                                  subfunction},
                        std::nullopt};
            }
            return {negative_response(
                        service_id, kUdsNegativeResponseCodeSubfunctionNotSupported),
                    std::nullopt};
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
            if (!state.security_unlocked) {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeSecurityAccessDenied),
                        std::nullopt};
            }

            did->second.value.assign(request.begin() + 3, request.end());
            return {std::vector<std::uint8_t>{
                         kUdsPositiveResponseWriteDataByIdentifier,
                         static_cast<std::uint8_t>((identifier >> 8U) & 0xFFU),
                         static_cast<std::uint8_t>(identifier & 0xFFU)},
                    std::nullopt};
        }

        case kUdsServiceReadDtcInformation: {
            if (request.size() < 2) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            const bool suppress = (request[1] & kSuppressPositiveResponseMask) != 0;
            const auto subfunction = static_cast<std::uint8_t>(request[1] & 0x7FU);
            if (subfunction != kUdsReportNumberOfDtcByStatusMask &&
                subfunction != kUdsReportDtcByStatusMask) {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeSubfunctionNotSupported),
                        std::nullopt};
            }
            if (request.size() != 3) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }

            const auto status_mask = request[2];
            std::vector<std::uint8_t> response{
                kUdsPositiveResponseReadDtcInformation, subfunction, kUdsDtcStatusAvailabilityMask};
            if (subfunction == kUdsReportNumberOfDtcByStatusMask) {
                const auto match_count = static_cast<std::uint16_t>(
                    std::count_if(dtcs.begin(), dtcs.end(), [status_mask](const auto& dtc) {
                        return matches_status_mask(dtc, status_mask);
                    }));
                response.push_back(kUdsDtcFormatIdentifier);
                append_u16(response, match_count);
            } else {
                for (const auto& dtc : dtcs) {
                    if (matches_status_mask(dtc, status_mask)) {
                        append_dtc_and_status(response, dtc);
                    }
                }
            }
            return {positive_response(suppress, std::move(response)), std::nullopt};
        }

        case kUdsServiceClearDiagnosticInformation: {
            if (request.size() != 4) {
                return {negative_response(service_id, kUdsNegativeResponseCodeIncorrectLength),
                        std::nullopt};
            }
            if (state.session != UdsDiagnosticSession::extended_session) {
                return {negative_response(
                            service_id,
                            kUdsNegativeResponseCodeServiceNotSupportedInActiveSession),
                        std::nullopt};
            }
            if (read_u24(request, 1) != kUdsAllDtcGroup) {
                return {negative_response(
                            service_id, kUdsNegativeResponseCodeRequestOutOfRange),
                        std::nullopt};
            }
            dtcs.clear();
            return {std::vector<std::uint8_t>{kUdsPositiveResponseClearDiagnosticInformation},
                    std::nullopt};
        }

        default:
            return {negative_response(service_id, kUdsNegativeResponseCodeServiceNotSupported),
                    std::nullopt};
    }
}

}  // namespace doip_uds
