#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/vehicle_identification.hpp"

TEST(VehicleIdentificationRequest, EncodesBroadcastRequestWithoutPayload) {
    const doip_uds::VehicleIdentificationRequestMessage request{
        doip_uds::VehicleIdentificationRequest{}};

    const auto encoded = doip_uds::encode_vehicle_identification_request(request);

    EXPECT_EQ(encoded.payload_type, doip_uds::kDoipPayloadTypeVehicleIdentificationRequest);
    EXPECT_TRUE(encoded.payload.empty());
}

TEST(VehicleIdentificationRequest, EncodesAndDecodesVinTargetedRequest) {
    const std::array<std::uint8_t, doip_uds::kDoipVinSize> vin{
        'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'};
    const doip_uds::VehicleIdentificationRequestMessage request{
        doip_uds::VehicleIdentificationRequestByVin{vin}};

    const auto encoded = doip_uds::encode_vehicle_identification_request(request);
    const auto decoded = doip_uds::decode_vehicle_identification_request(
        encoded.payload_type, encoded.payload.data(), encoded.payload.size());

    EXPECT_EQ(encoded.payload_type, 0x0003);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<doip_uds::VehicleIdentificationRequestByVin>(*decoded));
    EXPECT_EQ(std::get<doip_uds::VehicleIdentificationRequestByVin>(*decoded).vin, vin);
}

TEST(VehicleIdentificationRequest, EncodesAndDecodesEidTargetedRequest) {
    const std::array<std::uint8_t, doip_uds::kDoipEntityIdSize> eid{0x00, 0x11, 0x22,
                                                                    0x33, 0x44, 0x55};
    const doip_uds::VehicleIdentificationRequestMessage request{
        doip_uds::VehicleIdentificationRequestByEid{eid}};

    const auto encoded = doip_uds::encode_vehicle_identification_request(request);
    const auto decoded = doip_uds::decode_vehicle_identification_request(
        encoded.payload_type, encoded.payload.data(), encoded.payload.size());

    EXPECT_EQ(encoded.payload_type, 0x0002);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<doip_uds::VehicleIdentificationRequestByEid>(*decoded));
    EXPECT_EQ(std::get<doip_uds::VehicleIdentificationRequestByEid>(*decoded).eid, eid);
}

TEST(VehicleIdentificationRequest, DecodesBroadcastRequestWithEmptyPayload) {
    const auto decoded = doip_uds::decode_vehicle_identification_request(
        doip_uds::kDoipPayloadTypeVehicleIdentificationRequest, nullptr, 0);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<doip_uds::VehicleIdentificationRequest>(*decoded));
}

TEST(VehicleIdentificationRequest, RejectsUnknownTypeAndInvalidPayloadSizes) {
    const std::array<std::uint8_t, doip_uds::kDoipVinSize> vin{};

    EXPECT_FALSE(doip_uds::decode_vehicle_identification_request(0xFFFF, nullptr, 0).has_value());
    EXPECT_FALSE(doip_uds::decode_vehicle_identification_request(
                     doip_uds::kDoipPayloadTypeVehicleIdentificationRequest, vin.data(), vin.size())
                     .has_value());
    EXPECT_FALSE(doip_uds::decode_vehicle_identification_request(
                     doip_uds::kDoipPayloadTypeVehicleIdentificationRequestByVin,
                     vin.data(),
                     vin.size() - 1)
                     .has_value());
    EXPECT_FALSE(doip_uds::decode_vehicle_identification_request(
                     doip_uds::kDoipPayloadTypeVehicleIdentificationRequestByEid,
                     nullptr,
                     doip_uds::kDoipEntityIdSize)
                     .has_value());
}

TEST(VehicleIdentificationResponse, EncodesFieldsInNetworkByteOrder) {
    const doip_uds::VehicleIdentificationResponse response{
        {'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'},
        0x1001,
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        0x00,
        0x00};

    const auto encoded = doip_uds::encode_vehicle_identification_response(response);

    const std::array<std::uint8_t, doip_uds::kDoipVehicleIdentificationResponsePayloadSize> expected{
        'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1',
        0x10, 0x01, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x00};
    EXPECT_EQ(encoded, expected);
}

TEST(VehicleIdentificationResponse, DecodesAndRoundTripsAllFields) {
    const doip_uds::VehicleIdentificationResponse response{
        {'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'},
        0x1001,
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
        {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        0x00,
        0x00};
    const auto encoded = doip_uds::encode_vehicle_identification_response(response);

    const auto decoded =
        doip_uds::decode_vehicle_identification_response(encoded.data(), encoded.size());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->vin, response.vin);
    EXPECT_EQ(decoded->logical_address, response.logical_address);
    EXPECT_EQ(decoded->eid, response.eid);
    EXPECT_EQ(decoded->gid, response.gid);
    EXPECT_EQ(decoded->further_action, response.further_action);
    EXPECT_EQ(decoded->synchronization_status, response.synchronization_status);
}

TEST(VehicleIdentificationResponse, RejectsNullOrIncorrectlySizedPayload) {
    const std::array<std::uint8_t, doip_uds::kDoipVehicleIdentificationResponsePayloadSize> payload{};

    EXPECT_FALSE(doip_uds::decode_vehicle_identification_response(nullptr, payload.size())
                     .has_value());
    EXPECT_FALSE(doip_uds::decode_vehicle_identification_response(payload.data(), payload.size() - 1)
                     .has_value());
    EXPECT_FALSE(doip_uds::decode_vehicle_identification_response(payload.data(), payload.size() + 1)
                     .has_value());
}
