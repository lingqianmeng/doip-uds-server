#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "doip_uds/ecu_identity.hpp"

namespace doip_uds {

inline constexpr std::uint8_t kUdsServiceSessionControl = 0x10;
inline constexpr std::uint8_t kUdsServiceEcuReset = 0x11;
inline constexpr std::uint8_t kUdsServiceSecurityAccess = 0x27;
inline constexpr std::uint8_t kUdsServiceReadDataByIdentifier = 0x22;
inline constexpr std::uint8_t kUdsServiceWriteDataByIdentifier = 0x2E;
inline constexpr std::uint8_t kUdsServiceReadDtcInformation = 0x19;
inline constexpr std::uint8_t kUdsServiceClearDiagnosticInformation = 0x14;
inline constexpr std::uint8_t kUdsServiceTesterPresent = 0x3E;

inline constexpr std::uint8_t kUdsNegativeResponseCodeServiceNotSupported = 0x11;
inline constexpr std::uint8_t kUdsNegativeResponseCodeSubfunctionNotSupported = 0x12;
inline constexpr std::uint8_t kUdsNegativeResponseCodeIncorrectLength = 0x13;
inline constexpr std::uint8_t kUdsNegativeResponseCodeRequestSequenceError = 0x24;
inline constexpr std::uint8_t kUdsNegativeResponseCodeSecurityAccessDenied = 0x33;
inline constexpr std::uint8_t kUdsNegativeResponseCodeInvalidKey = 0x35;
inline constexpr std::uint8_t kUdsNegativeResponseCodeRequestOutOfRange = 0x31;
inline constexpr std::uint8_t kUdsNegativeResponseCodeServiceNotSupportedInActiveSession = 0x7E;
inline constexpr std::uint8_t kUdsDtcStatusAvailabilityMask = 0xFF;
inline constexpr std::uint8_t kUdsDtcFormatIdentifier = 0x01;
inline constexpr std::uint32_t kUdsAllDtcGroup = 0xFFFFFFU;

inline constexpr std::uint16_t kUdsP2ServerMaximumMilliseconds = 50;
inline constexpr std::uint16_t kUdsP2StarServerMaximum10Milliseconds = 500;
inline constexpr std::uint16_t kUdsVinDataIdentifier = kVinDataIdentifier;

enum class UdsDiagnosticSession : std::uint8_t {
    default_session = 0x01,
    extended_session = 0x03,
};

enum class UdsEcuResetType : std::uint8_t {
    hard_reset = 0x01,
};

struct UdsConnectionState {
    UdsDiagnosticSession session{UdsDiagnosticSession::default_session};
    std::optional<std::uint16_t> pending_security_seed;
    std::uint16_t next_security_seed{0x1234};
    bool security_unlocked{};
};

struct UdsDataIdentifier {
    std::vector<std::uint8_t> value;
    bool readable{};
    bool writable{};
};

using UdsDataIdentifierStore = std::map<std::uint16_t, UdsDataIdentifier>;

struct UdsDispatchResult {
    std::optional<std::vector<std::uint8_t>> response;
    std::optional<UdsEcuResetType> reset_requested;
};

/// Creates the process-wide DID store, including the configured VIN DID.
UdsDataIdentifierStore make_uds_data_identifier_store(const EcuIdentity& identity);

/// Creates the process-wide DTC store from the configured synthetic DTCs.
UdsDtcStore make_uds_dtc_store(const EcuIdentity& identity);

/// Dispatches one non-empty UDS request and updates the connection's UDS state.
UdsDispatchResult dispatch_uds_request(const std::vector<std::uint8_t>& request,
                                      UdsConnectionState& state,
                                      UdsDataIdentifierStore& data_identifiers,
                                      UdsDtcStore& dtcs);

}  // namespace doip_uds
