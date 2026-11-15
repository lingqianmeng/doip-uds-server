#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace doip_uds {

inline constexpr std::uint16_t kDoipPayloadTypeDiagnosticMessage = 0x8001;
inline constexpr std::uint16_t kDoipPayloadTypeDiagnosticMessagePositiveAck = 0x8002;
inline constexpr std::uint16_t kDoipPayloadTypeDiagnosticMessageNegativeAck = 0x8003;

inline constexpr std::size_t kDoipDiagnosticMessageAddressSize = 4;
inline constexpr std::size_t kDoipDiagnosticMessageMinimumPayloadSize = 5;
inline constexpr std::size_t kDoipDiagnosticMessageAckPayloadSize = 5;
inline constexpr std::uint8_t kDoipDiagnosticMessagePositiveAckCode = 0x00;

enum class DiagnosticMessageNackCode : std::uint8_t {
    invalid_source_address = 0x02,
    unknown_target_address = 0x03,
};

struct DiagnosticMessage {
    std::uint16_t source_logical_address{};
    std::uint16_t target_logical_address{};
    std::vector<std::uint8_t> user_data;
};

struct DiagnosticMessageHandlingResult {
    std::uint16_t response_payload_type{};
    std::uint8_t response_code{};
    std::vector<std::uint8_t> response_payload;
    std::optional<DiagnosticMessage> accepted_message;
};

/// Encodes a DoIP Diagnostic Message payload.
std::vector<std::uint8_t> encode_diagnostic_message(const DiagnosticMessage& message);

/// Decodes a Diagnostic Message payload containing two addresses and UDS data.
std::optional<DiagnosticMessage> decode_diagnostic_message(
    const std::uint8_t* payload,
    std::size_t payload_size);

/// Encodes a positive acknowledgement with response-direction logical addresses.
std::vector<std::uint8_t> encode_diagnostic_message_positive_ack(
    std::uint16_t source_logical_address,
    std::uint16_t target_logical_address);

/// Encodes a negative acknowledgement with response-direction logical addresses.
std::vector<std::uint8_t> encode_diagnostic_message_negative_ack(
    std::uint16_t source_logical_address,
    std::uint16_t target_logical_address,
    DiagnosticMessageNackCode nack_code);

/// Applies the current routing-activation and ECU-target policy to a request.
DiagnosticMessageHandlingResult handle_diagnostic_message(
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::optional<std::uint16_t> active_tester_logical_address,
    std::uint16_t entity_logical_address);

}  // namespace doip_uds
