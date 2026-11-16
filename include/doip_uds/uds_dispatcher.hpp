#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace doip_uds {

inline constexpr std::uint8_t kUdsServiceSessionControl = 0x10;
inline constexpr std::uint8_t kUdsServiceEcuReset = 0x11;
inline constexpr std::uint8_t kUdsServiceTesterPresent = 0x3E;

inline constexpr std::uint8_t kUdsNegativeResponseCodeServiceNotSupported = 0x11;
inline constexpr std::uint8_t kUdsNegativeResponseCodeSubfunctionNotSupported = 0x12;
inline constexpr std::uint8_t kUdsNegativeResponseCodeIncorrectLength = 0x13;

inline constexpr std::uint16_t kUdsP2ServerMaximumMilliseconds = 50;
inline constexpr std::uint16_t kUdsP2StarServerMaximum10Milliseconds = 500;

enum class UdsDiagnosticSession : std::uint8_t {
    default_session = 0x01,
    extended_session = 0x03,
};

enum class UdsEcuResetType : std::uint8_t {
    hard_reset = 0x01,
};

struct UdsConnectionState {
    UdsDiagnosticSession session{UdsDiagnosticSession::default_session};
};

struct UdsDispatchResult {
    std::optional<std::vector<std::uint8_t>> response;
    std::optional<UdsEcuResetType> reset_requested;
};

/// Dispatches one non-empty UDS request and updates the connection's UDS state.
UdsDispatchResult dispatch_uds_request(const std::vector<std::uint8_t>& request,
                                      UdsConnectionState& state);

}  // namespace doip_uds
