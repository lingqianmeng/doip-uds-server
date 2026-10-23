#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "doip_uds/doip_header.hpp"

TEST(DoipHeader, EncodesFieldsInNetworkByteOrder) {
    const doip_uds::DoipHeader header{0x02, 0x8001, 0x12345678};

    const auto encoded = doip_uds::encode_doip_header(header);

    const std::array<std::uint8_t, doip_uds::kDoipGenericHeaderSize> expected{
        0x02, 0xFD, 0x80, 0x01, 0x12, 0x34, 0x56, 0x78};
    EXPECT_EQ(encoded, expected);
}

TEST(DoipHeader, DecodesFieldsInNetworkByteOrder) {
    const std::array<std::uint8_t, doip_uds::kDoipGenericHeaderSize> encoded{
        0x03, 0xFC, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00};

    const auto decoded = doip_uds::decode_doip_header(encoded.data(), encoded.size());

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->protocol_version, 0x03);
    EXPECT_EQ(decoded->payload_type, 0x0001);
    EXPECT_EQ(decoded->payload_length, 0x00000100);
}

TEST(DoipHeader, RejectsTruncatedInput) {
    const std::array<std::uint8_t, doip_uds::kDoipGenericHeaderSize - 1> encoded{};

    EXPECT_FALSE(doip_uds::decode_doip_header(encoded.data(), encoded.size()).has_value());
}

TEST(DoipHeader, RejectsNullInput) {
    EXPECT_FALSE(doip_uds::decode_doip_header(nullptr, doip_uds::kDoipGenericHeaderSize)
                     .has_value());
}

TEST(DoipHeader, RejectsInvalidProtocolVersionInverse) {
    const std::array<std::uint8_t, doip_uds::kDoipGenericHeaderSize> encoded{
        0x02, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(doip_uds::decode_doip_header(encoded.data(), encoded.size()).has_value());
}
