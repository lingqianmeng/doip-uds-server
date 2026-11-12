#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "doip_uds/doip_header.hpp"
#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/vehicle_identification.hpp"
#include "doip_uds/vehicle_identification_handler.hpp"

namespace {

const doip_uds::EcuIdentity kIdentity{
    {'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'},
    0x1001,
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};

std::vector<std::uint8_t> make_request(
    const doip_uds::VehicleIdentificationRequestMessage& request) {
    const auto encoded_payload = doip_uds::encode_vehicle_identification_request(request);
    const doip_uds::DoipHeader header{
        doip_uds::kDoipDefaultProtocolVersionForVehicleIdentificationRequest,
        encoded_payload.payload_type,
        static_cast<std::uint32_t>(encoded_payload.payload.size())};
    const auto encoded_header = doip_uds::encode_doip_header(header);

    std::vector<std::uint8_t> datagram(encoded_header.begin(), encoded_header.end());
    datagram.insert(datagram.end(), encoded_payload.payload.begin(), encoded_payload.payload.end());
    return datagram;
}

void expect_identity_response(const std::vector<std::uint8_t>& datagram) {
    ASSERT_EQ(datagram.size(),
              doip_uds::kDoipGenericHeaderSize +
                  doip_uds::kDoipVehicleIdentificationResponsePayloadSize);
    const auto header = doip_uds::decode_doip_header(datagram.data(), datagram.size());
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->protocol_version, doip_uds::kDoipProtocolVersion);
    EXPECT_EQ(header->payload_type,
              doip_uds::kDoipPayloadTypeVehicleIdentificationResponse);
    EXPECT_EQ(header->payload_length, doip_uds::kDoipVehicleIdentificationResponsePayloadSize);

    const auto response = doip_uds::decode_vehicle_identification_response(
        datagram.data() + doip_uds::kDoipGenericHeaderSize,
        datagram.size() - doip_uds::kDoipGenericHeaderSize);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->vin, kIdentity.vin);
    EXPECT_EQ(response->logical_address, kIdentity.logical_address);
    EXPECT_EQ(response->eid, kIdentity.eid);
    EXPECT_EQ(response->gid, kIdentity.gid);
    EXPECT_EQ(response->further_action, 0x00);
    EXPECT_EQ(response->synchronization_status, 0x00);
}

}  // namespace

TEST(VehicleIdentificationHandler, BuildsVehicleAnnouncementWithConfiguredIdentity) {
    const auto announcement =
        doip_uds::make_vehicle_identification_response_datagram(kIdentity);

    expect_identity_response(announcement);
}

TEST(VehicleIdentificationHandler, RespondsToBroadcastRequest) {
    const auto request =
        make_request(doip_uds::VehicleIdentificationRequest{});

    const auto response =
        doip_uds::handle_vehicle_identification_datagram(kIdentity, request);

    ASSERT_TRUE(response.has_value());
    expect_identity_response(*response);
}

TEST(VehicleIdentificationHandler, RespondsToMatchingVinAndEidRequests) {
    const std::array<std::uint8_t, doip_uds::kDoipVinSize> other_vin{
        'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '2'};
    const std::array<std::uint8_t, doip_uds::kDoipEntityIdSize> other_eid{
        0x00, 0x11, 0x22, 0x33, 0x44, 0x56};

    const std::array<std::uint8_t, doip_uds::kDoipVinSize> vin = kIdentity.vin;
    const std::array<std::uint8_t, doip_uds::kDoipEntityIdSize> eid = kIdentity.eid;
    for (const auto& request : {
             doip_uds::VehicleIdentificationRequestMessage{
                 doip_uds::VehicleIdentificationRequestByVin{vin}},
             doip_uds::VehicleIdentificationRequestMessage{
                 doip_uds::VehicleIdentificationRequestByEid{eid}}}) {
        const auto datagram = make_request(request);
        const auto response =
            doip_uds::handle_vehicle_identification_datagram(kIdentity, datagram);
        ASSERT_TRUE(response.has_value());
        expect_identity_response(*response);
    }

    const auto nonmatching_vin = make_request(
        doip_uds::VehicleIdentificationRequestByVin{other_vin});
    const auto nonmatching_eid = make_request(
        doip_uds::VehicleIdentificationRequestByEid{other_eid});
    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, nonmatching_vin).has_value());
    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, nonmatching_eid).has_value());
}

TEST(VehicleIdentificationHandler, IgnoresMalformedOrUnsupportedDatagrams) {
    const auto valid_request = make_request(doip_uds::VehicleIdentificationRequest{});
    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, {}).has_value());
    EXPECT_FALSE(doip_uds::handle_vehicle_identification_datagram(
                     kIdentity,
                     std::vector<std::uint8_t>(valid_request.begin(), valid_request.begin() + 7))
                     .has_value());

    auto incorrect_length = valid_request;
    incorrect_length[7] = 0x01;
    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, incorrect_length).has_value());

    auto incorrect_version = valid_request;
    incorrect_version[0] = 0x02;
    incorrect_version[1] = 0xFD;
    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, incorrect_version).has_value());

    auto invalid_version_inverse = valid_request;
    invalid_version_inverse[1] = 0xFF;
    EXPECT_FALSE(doip_uds::handle_vehicle_identification_datagram(
                     kIdentity, invalid_version_inverse)
                     .has_value());

    auto unknown_payload_type = valid_request;
    unknown_payload_type[2] = 0xFF;
    unknown_payload_type[3] = 0xFF;
    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, unknown_payload_type)
            .has_value());
}

TEST(VehicleIdentificationHandler, RejectsExtraBytesBeyondDeclaredPayload) {
    auto datagram = make_request(doip_uds::VehicleIdentificationRequest{});
    datagram.push_back(0x00);

    EXPECT_FALSE(
        doip_uds::handle_vehicle_identification_datagram(kIdentity, datagram).has_value());
}
