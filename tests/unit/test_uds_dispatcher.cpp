#include "doip_uds/uds_dispatcher.hpp"

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

TEST(UdsDispatcher, HandlesDefaultAndExtendedSessionControl) {
    doip_uds::UdsConnectionState state;

    const auto extended = doip_uds::dispatch_uds_request({0x10, 0x03}, state);
    ASSERT_TRUE(extended.response.has_value());
    EXPECT_EQ(*extended.response,
              (std::vector<std::uint8_t>{0x50, 0x03, 0x00, 0x32, 0x01, 0xF4}));
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::extended_session);

    const auto default_session = doip_uds::dispatch_uds_request({0x10, 0x01}, state);
    ASSERT_TRUE(default_session.response.has_value());
    EXPECT_EQ(*default_session.response,
              (std::vector<std::uint8_t>{0x50, 0x01, 0x00, 0x32, 0x01, 0xF4}));
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, SuppressesPositiveSessionResponseButAppliesTransition) {
    doip_uds::UdsConnectionState state;

    const auto result = doip_uds::dispatch_uds_request({0x10, 0x83}, state);

    EXPECT_FALSE(result.response.has_value());
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::extended_session);
}

TEST(UdsDispatcher, HandlesTesterPresentAndSuppression) {
    doip_uds::UdsConnectionState state;

    const auto response = doip_uds::dispatch_uds_request({0x3E, 0x00}, state);
    ASSERT_TRUE(response.response.has_value());
    EXPECT_EQ(*response.response, (std::vector<std::uint8_t>{0x7E, 0x00}));

    const auto suppressed = doip_uds::dispatch_uds_request({0x3E, 0x80}, state);
    EXPECT_FALSE(suppressed.response.has_value());
}

TEST(UdsDispatcher, SimulatesHardResetAndReturnsToDefaultSession) {
    doip_uds::UdsConnectionState state;
    state.session = doip_uds::UdsDiagnosticSession::extended_session;

    const auto result = doip_uds::dispatch_uds_request({0x11, 0x01}, state);

    ASSERT_TRUE(result.response.has_value());
    EXPECT_EQ(*result.response, (std::vector<std::uint8_t>{0x51, 0x01}));
    ASSERT_TRUE(result.reset_requested.has_value());
    EXPECT_EQ(*result.reset_requested, doip_uds::UdsEcuResetType::hard_reset);
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, SuppressedHardResetStillResetsState) {
    doip_uds::UdsConnectionState state;
    state.session = doip_uds::UdsDiagnosticSession::extended_session;

    const auto result = doip_uds::dispatch_uds_request({0x11, 0x81}, state);

    EXPECT_FALSE(result.response.has_value());
    ASSERT_TRUE(result.reset_requested.has_value());
    EXPECT_EQ(*result.reset_requested, doip_uds::UdsEcuResetType::hard_reset);
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, ReturnsNegativeResponsesForUnsupportedServicesAndSubfunctions) {
    doip_uds::UdsConnectionState state;

    const auto unsupported_service = doip_uds::dispatch_uds_request({0x22}, state);
    ASSERT_TRUE(unsupported_service.response.has_value());
    EXPECT_EQ(*unsupported_service.response, (std::vector<std::uint8_t>{0x7F, 0x22, 0x11}));

    const auto unsupported_session = doip_uds::dispatch_uds_request({0x10, 0x02}, state);
    ASSERT_TRUE(unsupported_session.response.has_value());
    EXPECT_EQ(*unsupported_session.response, (std::vector<std::uint8_t>{0x7F, 0x10, 0x12}));

    const auto unsupported_tester_present = doip_uds::dispatch_uds_request({0x3E, 0x01}, state);
    ASSERT_TRUE(unsupported_tester_present.response.has_value());
    EXPECT_EQ(*unsupported_tester_present.response,
              (std::vector<std::uint8_t>{0x7F, 0x3E, 0x12}));

    const auto unsupported_reset = doip_uds::dispatch_uds_request({0x11, 0x03}, state);
    ASSERT_TRUE(unsupported_reset.response.has_value());
    EXPECT_EQ(*unsupported_reset.response, (std::vector<std::uint8_t>{0x7F, 0x11, 0x12}));
}

TEST(UdsDispatcher, ReturnsIncorrectLengthForMalformedServiceRequests) {
    doip_uds::UdsConnectionState state;

    const std::vector<std::vector<std::uint8_t>> requests{
        {0x10}, {0x10, 0x01, 0x00}, {0x3E}, {0x3E, 0x00, 0x00}, {0x11}, {0x11, 0x01, 0x00}};
    for (const auto& request : requests) {
        const auto result = doip_uds::dispatch_uds_request(request, state);
        ASSERT_TRUE(result.response.has_value());
        EXPECT_EQ((*result.response)[0], 0x7F);
        EXPECT_EQ((*result.response)[1], request[0]);
        EXPECT_EQ((*result.response)[2], doip_uds::kUdsNegativeResponseCodeIncorrectLength);
    }
}

TEST(UdsDispatcher, RejectsEmptyRequestWithoutServiceIdentifier) {
    doip_uds::UdsConnectionState state;
    EXPECT_THROW(doip_uds::dispatch_uds_request({}, state), std::invalid_argument);
}

}  // namespace
