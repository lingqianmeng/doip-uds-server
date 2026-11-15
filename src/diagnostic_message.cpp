#include "doip_uds/diagnostic_message.hpp"

#include <stdexcept>

namespace doip_uds {
namespace {

std::uint16_t read_u16(const std::uint8_t* input) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(input[0]) << 8U) | static_cast<std::uint16_t>(input[1]));
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

}  // namespace

std::vector<std::uint8_t> encode_diagnostic_message(const DiagnosticMessage& message) {
    if (message.user_data.empty()) {
        throw std::invalid_argument("DoIP diagnostic message requires non-empty user data");
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(kDoipDiagnosticMessageAddressSize + message.user_data.size());
    append_u16(payload, message.source_logical_address);
    append_u16(payload, message.target_logical_address);
    payload.insert(payload.end(), message.user_data.begin(), message.user_data.end());
    return payload;
}

std::optional<DiagnosticMessage> decode_diagnostic_message(const std::uint8_t* payload,
                                                           std::size_t payload_size) {
    if (payload == nullptr || payload_size < kDoipDiagnosticMessageMinimumPayloadSize) {
        return std::nullopt;
    }

    return DiagnosticMessage{read_u16(payload),
                             read_u16(payload + 2),
                             std::vector<std::uint8_t>(payload + kDoipDiagnosticMessageAddressSize,
                                                       payload + payload_size)};
}

std::vector<std::uint8_t> encode_diagnostic_message_positive_ack(
    std::uint16_t source_logical_address,
    std::uint16_t target_logical_address) {
    std::vector<std::uint8_t> payload;
    payload.reserve(kDoipDiagnosticMessageAckPayloadSize);
    append_u16(payload, source_logical_address);
    append_u16(payload, target_logical_address);
    payload.push_back(kDoipDiagnosticMessagePositiveAckCode);
    return payload;
}

std::vector<std::uint8_t> encode_diagnostic_message_negative_ack(
    std::uint16_t source_logical_address,
    std::uint16_t target_logical_address,
    DiagnosticMessageNackCode nack_code) {
    std::vector<std::uint8_t> payload;
    payload.reserve(kDoipDiagnosticMessageAckPayloadSize);
    append_u16(payload, source_logical_address);
    append_u16(payload, target_logical_address);
    payload.push_back(static_cast<std::uint8_t>(nack_code));
    return payload;
}

DiagnosticMessageHandlingResult handle_diagnostic_message(
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::optional<std::uint16_t> active_tester_logical_address,
    std::uint16_t entity_logical_address) {
    const auto request = decode_diagnostic_message(payload, payload_size);
    if (!request) {
        throw std::invalid_argument("diagnostic message has an invalid payload length");
    }

    if (!active_tester_logical_address ||
        request->source_logical_address != *active_tester_logical_address) {
        const auto nack_code = DiagnosticMessageNackCode::invalid_source_address;
        return {kDoipPayloadTypeDiagnosticMessageNegativeAck,
                static_cast<std::uint8_t>(nack_code),
                encode_diagnostic_message_negative_ack(
                    entity_logical_address, request->source_logical_address, nack_code),
                std::nullopt};
    }

    if (request->target_logical_address != entity_logical_address) {
        const auto nack_code = DiagnosticMessageNackCode::unknown_target_address;
        return {kDoipPayloadTypeDiagnosticMessageNegativeAck,
                static_cast<std::uint8_t>(nack_code),
                encode_diagnostic_message_negative_ack(
                    entity_logical_address, request->source_logical_address, nack_code),
                std::nullopt};
    }

    return {kDoipPayloadTypeDiagnosticMessagePositiveAck,
            kDoipDiagnosticMessagePositiveAckCode,
            encode_diagnostic_message_positive_ack(
                entity_logical_address, request->source_logical_address),
            request};
}

}  // namespace doip_uds
