#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/diagnostic_message.hpp"

TEST(DiagnosticMessage, EncodesAndDecodesAddressesAndUserData) {
    const doip_uds::DiagnosticMessage request{
        0x0E00, 0x1001, {0x22, 0xF1, 0x90}};
    const auto encoded = doip_uds::encode_diagnostic_message(request);

    const std::vector<std::uint8_t> expected{0x0E, 0x00, 0x10, 0x01, 0x22, 0xF1, 0x90};
    EXPECT_EQ(encoded, expected);

    const auto decoded = doip_uds::decode_diagnostic_message(encoded.data(), encoded.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->source_logical_address, request.source_logical_address);
    EXPECT_EQ(decoded->target_logical_address, request.target_logical_address);
    EXPECT_EQ(decoded->user_data, request.user_data);
}

TEST(DiagnosticMessage, RejectsNullTruncatedAndEmptyUserData) {
    const std::array<std::uint8_t, 4> address_fields{};

    EXPECT_FALSE(doip_uds::decode_diagnostic_message(nullptr, 5).has_value());
    EXPECT_FALSE(
        doip_uds::decode_diagnostic_message(address_fields.data(), address_fields.size())
            .has_value());
    EXPECT_THROW(
        doip_uds::encode_diagnostic_message({0x0E00, 0x1001, {}}),
        std::invalid_argument);
}

TEST(DiagnosticMessage, EncodesPositiveAcknowledgement) {
    const auto encoded = doip_uds::encode_diagnostic_message_positive_ack(0x1001, 0x0E00);

    const std::vector<std::uint8_t> expected{0x10, 0x01, 0x0E, 0x00, 0x00};
    EXPECT_EQ(encoded, expected);
}

TEST(DiagnosticMessage, EncodesNegativeAcknowledgement) {
    const auto encoded = doip_uds::encode_diagnostic_message_negative_ack(
        0x1001, 0x0E00, doip_uds::DiagnosticMessageNackCode::unknown_target_address);

    const std::vector<std::uint8_t> expected{0x10, 0x01, 0x0E, 0x00, 0x03};
    EXPECT_EQ(encoded, expected);
}

TEST(DiagnosticMessageHandler, RejectsMessageBeforeRoutingActivation) {
    const auto payload = doip_uds::encode_diagnostic_message(
        {0x0E00, 0x1001, {0x3E, 0x00}});

    const auto result =
        doip_uds::handle_diagnostic_message(payload.data(), payload.size(), std::nullopt, 0x1001);

    EXPECT_EQ(result.response_payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessageNegativeAck);
    EXPECT_EQ(result.response_code,
              static_cast<std::uint8_t>(
                  doip_uds::DiagnosticMessageNackCode::invalid_source_address));
    EXPECT_EQ(result.response_payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x02}));
    EXPECT_FALSE(result.accepted_message.has_value());
}

TEST(DiagnosticMessageHandler, RejectsSourceAddressNotActivatedOnConnection) {
    const auto payload = doip_uds::encode_diagnostic_message(
        {0x0E01, 0x1001, {0x3E, 0x00}});

    const auto result =
        doip_uds::handle_diagnostic_message(payload.data(), payload.size(), 0x0E00, 0x1001);

    EXPECT_EQ(result.response_payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessageNegativeAck);
    EXPECT_EQ(result.response_code,
              static_cast<std::uint8_t>(
                  doip_uds::DiagnosticMessageNackCode::invalid_source_address));
    EXPECT_FALSE(result.accepted_message.has_value());
}

TEST(DiagnosticMessageHandler, RejectsUnknownTargetAddress) {
    const auto payload = doip_uds::encode_diagnostic_message(
        {0x0E00, 0x1002, {0x3E, 0x00}});

    const auto result =
        doip_uds::handle_diagnostic_message(payload.data(), payload.size(), 0x0E00, 0x1001);

    EXPECT_EQ(result.response_payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessageNegativeAck);
    EXPECT_EQ(result.response_code,
              static_cast<std::uint8_t>(
                  doip_uds::DiagnosticMessageNackCode::unknown_target_address));
    EXPECT_EQ(result.response_payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x03}));
    EXPECT_FALSE(result.accepted_message.has_value());
}

TEST(DiagnosticMessageHandler, AcknowledgesAndPreservesAcceptedUserData) {
    const doip_uds::DiagnosticMessage request{0x0E00, 0x1001, {0x22, 0xF1, 0x90}};
    const auto payload = doip_uds::encode_diagnostic_message(request);

    const auto result =
        doip_uds::handle_diagnostic_message(payload.data(), payload.size(), 0x0E00, 0x1001);

    EXPECT_EQ(result.response_payload_type,
              doip_uds::kDoipPayloadTypeDiagnosticMessagePositiveAck);
    EXPECT_EQ(result.response_code, doip_uds::kDoipDiagnosticMessagePositiveAckCode);
    EXPECT_EQ(result.response_payload,
              (std::vector<std::uint8_t>{0x10, 0x01, 0x0E, 0x00, 0x00}));
    ASSERT_TRUE(result.accepted_message.has_value());
    EXPECT_EQ(result.accepted_message->source_logical_address, request.source_logical_address);
    EXPECT_EQ(result.accepted_message->target_logical_address, request.target_logical_address);
    EXPECT_EQ(result.accepted_message->user_data, request.user_data);
}
