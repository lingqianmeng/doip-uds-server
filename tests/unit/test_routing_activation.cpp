#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/routing_activation.hpp"

TEST(RoutingActivationRequest, EncodesAndDecodesDefaultRequest) {
    const doip_uds::RoutingActivationRequest request{0x0E00, 0x00, std::nullopt};
    const auto encoded = doip_uds::encode_routing_activation_request(request);

    const std::vector<std::uint8_t> expected{
        0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(encoded, expected);

    const auto decoded =
        doip_uds::decode_routing_activation_request(encoded.data(), encoded.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->tester_logical_address, request.tester_logical_address);
    EXPECT_EQ(decoded->activation_type, request.activation_type);
    EXPECT_FALSE(decoded->oem_data.has_value());
}

TEST(RoutingActivationRequest, PreservesOptionalOemData) {
    const doip_uds::RoutingActivationRequest request{
        0x0EFF, 0x00, std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0x44}};
    const auto encoded = doip_uds::encode_routing_activation_request(request);
    const auto decoded =
        doip_uds::decode_routing_activation_request(encoded.data(), encoded.size());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->tester_logical_address, request.tester_logical_address);
    EXPECT_EQ(decoded->activation_type, request.activation_type);
    EXPECT_EQ(decoded->oem_data, request.oem_data);
}

TEST(RoutingActivationRequest, RejectsNullAndInvalidPayloadSizes) {
    const std::array<std::uint8_t, 12> payload{};

    EXPECT_FALSE(doip_uds::decode_routing_activation_request(nullptr, 7).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_request(payload.data(), 6).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_request(payload.data(), 8).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_request(payload.data(), 10).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_request(payload.data(), 12).has_value());
}

TEST(RoutingActivationResponse, EncodesAndDecodesAllFields) {
    const doip_uds::RoutingActivationResponse response{
        0x0E00,
        0x1001,
        doip_uds::RoutingActivationResponseCode::routing_successfully_activated,
        std::nullopt};
    const auto encoded = doip_uds::encode_routing_activation_response(response);

    const std::vector<std::uint8_t> expected{
        0x0E, 0x00, 0x10, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(encoded, expected);

    const auto decoded =
        doip_uds::decode_routing_activation_response(encoded.data(), encoded.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->tester_logical_address, response.tester_logical_address);
    EXPECT_EQ(decoded->entity_logical_address, response.entity_logical_address);
    EXPECT_EQ(decoded->response_code, response.response_code);
    EXPECT_FALSE(decoded->oem_data.has_value());
}

TEST(RoutingActivationResponse, PreservesOptionalOemDataAndUnknownCodes) {
    const doip_uds::RoutingActivationResponse response{
        0x0E00,
        0x1001,
        static_cast<doip_uds::RoutingActivationResponseCode>(0xFE),
        std::array<std::uint8_t, 4>{0xAA, 0xBB, 0xCC, 0xDD}};
    const auto encoded = doip_uds::encode_routing_activation_response(response);
    const auto decoded =
        doip_uds::decode_routing_activation_response(encoded.data(), encoded.size());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->response_code, response.response_code);
    EXPECT_EQ(decoded->oem_data, response.oem_data);
}

TEST(RoutingActivationResponse, RejectsNullAndInvalidPayloadSizes) {
    const std::array<std::uint8_t, 14> payload{};

    EXPECT_FALSE(doip_uds::decode_routing_activation_response(nullptr, 9).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_response(payload.data(), 8).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_response(payload.data(), 10).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_response(payload.data(), 12).has_value());
    EXPECT_FALSE(doip_uds::decode_routing_activation_response(payload.data(), 14).has_value());
}

TEST(RoutingActivationSession, ActivatesDefaultTypeForTesterAddressRange) {
    doip_uds::RoutingActivationSession session;
    const doip_uds::RoutingActivationRequest request{0x0E00, 0x00, std::nullopt};

    const auto response = session.handle_request(request, 0x1001, false);

    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::routing_successfully_activated);
    EXPECT_EQ(session.active_tester_logical_address(), 0x0E00);
}

TEST(RoutingActivationSession, RejectsAddressesOutsideTestEquipmentRange) {
    for (const std::uint16_t address : std::array<std::uint16_t, 2>{0x0DFF, 0x1000}) {
        doip_uds::RoutingActivationSession session;
        const auto response =
            session.handle_request({address, 0x00, std::nullopt}, 0x1001, false);

        EXPECT_EQ(response.response_code,
                  doip_uds::RoutingActivationResponseCode::unknown_source_address);
        EXPECT_FALSE(session.active_tester_logical_address().has_value());
    }
}

TEST(RoutingActivationSession, RejectsUnsupportedActivationType) {
    doip_uds::RoutingActivationSession session;
    const auto response =
        session.handle_request({0x0E00, 0x01, std::nullopt}, 0x1001, false);

    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::unsupported_activation_type);
    EXPECT_FALSE(session.active_tester_logical_address().has_value());
}

TEST(RoutingActivationSession, RejectsTesterAddressActiveOnAnotherSocket) {
    doip_uds::RoutingActivationSession session;
    const auto response =
        session.handle_request({0x0E00, 0x00, std::nullopt}, 0x1001, true);

    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::source_address_active_on_another_socket);
    EXPECT_FALSE(session.active_tester_logical_address().has_value());
}

TEST(RoutingActivationSession, RejectsDifferentAddressOnAlreadyActivatedSocket) {
    doip_uds::RoutingActivationSession session;
    ASSERT_EQ(session.handle_request({0x0E00, 0x00, std::nullopt}, 0x1001, false).response_code,
              doip_uds::RoutingActivationResponseCode::routing_successfully_activated);

    const auto response =
        session.handle_request({0x0E01, 0x00, std::nullopt}, 0x1001, false);

    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::
                  different_source_address_on_activated_socket);
    EXPECT_EQ(session.active_tester_logical_address(), 0x0E00);
}

TEST(RoutingActivationSession, AllowsRepeatedRequestOnSameSocketAndAddress) {
    doip_uds::RoutingActivationSession session;
    const doip_uds::RoutingActivationRequest request{0x0E00, 0x00, std::nullopt};
    ASSERT_EQ(session.handle_request(request, 0x1001, false).response_code,
              doip_uds::RoutingActivationResponseCode::routing_successfully_activated);

    const auto response = session.handle_request(request, 0x1001, false);

    EXPECT_EQ(response.response_code,
              doip_uds::RoutingActivationResponseCode::routing_successfully_activated);
}
