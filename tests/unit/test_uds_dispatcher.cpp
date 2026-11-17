#include "doip_uds/uds_dispatcher.hpp"

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

doip_uds::UdsDataIdentifierStore make_test_data_identifiers() {
    return {
        {0xF190,
         {{'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
           '1'},
          true,
          false}},
        {0xF1A0, {{0x00, 0x00, 0x00, 0x00}, true, true}},
    };
}

doip_uds::UdsDispatchResult dispatch(const std::vector<std::uint8_t>& request,
                                    doip_uds::UdsConnectionState& state) {
    auto data_identifiers = make_test_data_identifiers();
    return doip_uds::dispatch_uds_request(request, state, data_identifiers);
}

TEST(UdsDispatcher, HandlesDefaultAndExtendedSessionControl) {
    doip_uds::UdsConnectionState state;

    const auto extended = dispatch({0x10, 0x03}, state);
    ASSERT_TRUE(extended.response.has_value());
    EXPECT_EQ(*extended.response,
              (std::vector<std::uint8_t>{0x50, 0x03, 0x00, 0x32, 0x01, 0xF4}));
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::extended_session);

    const auto default_session = dispatch({0x10, 0x01}, state);
    ASSERT_TRUE(default_session.response.has_value());
    EXPECT_EQ(*default_session.response,
              (std::vector<std::uint8_t>{0x50, 0x01, 0x00, 0x32, 0x01, 0xF4}));
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, SuppressesPositiveSessionResponseButAppliesTransition) {
    doip_uds::UdsConnectionState state;

    const auto result = dispatch({0x10, 0x83}, state);

    EXPECT_FALSE(result.response.has_value());
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::extended_session);
}

TEST(UdsDispatcher, HandlesTesterPresentAndSuppression) {
    doip_uds::UdsConnectionState state;

    const auto response = dispatch({0x3E, 0x00}, state);
    ASSERT_TRUE(response.response.has_value());
    EXPECT_EQ(*response.response, (std::vector<std::uint8_t>{0x7E, 0x00}));

    const auto suppressed = dispatch({0x3E, 0x80}, state);
    EXPECT_FALSE(suppressed.response.has_value());
}

TEST(UdsDispatcher, SimulatesHardResetAndReturnsToDefaultSession) {
    doip_uds::UdsConnectionState state;
    state.session = doip_uds::UdsDiagnosticSession::extended_session;

    const auto result = dispatch({0x11, 0x01}, state);

    ASSERT_TRUE(result.response.has_value());
    EXPECT_EQ(*result.response, (std::vector<std::uint8_t>{0x51, 0x01}));
    ASSERT_TRUE(result.reset_requested.has_value());
    EXPECT_EQ(*result.reset_requested, doip_uds::UdsEcuResetType::hard_reset);
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, SuppressedHardResetStillResetsState) {
    doip_uds::UdsConnectionState state;
    state.session = doip_uds::UdsDiagnosticSession::extended_session;

    const auto result = dispatch({0x11, 0x81}, state);

    EXPECT_FALSE(result.response.has_value());
    ASSERT_TRUE(result.reset_requested.has_value());
    EXPECT_EQ(*result.reset_requested, doip_uds::UdsEcuResetType::hard_reset);
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, ReturnsNegativeResponsesForUnsupportedServicesAndSubfunctions) {
    doip_uds::UdsConnectionState state;

    const auto unsupported_service = dispatch({0x99}, state);
    ASSERT_TRUE(unsupported_service.response.has_value());
    EXPECT_EQ(*unsupported_service.response, (std::vector<std::uint8_t>{0x7F, 0x99, 0x11}));

    const auto unsupported_session = dispatch({0x10, 0x02}, state);
    ASSERT_TRUE(unsupported_session.response.has_value());
    EXPECT_EQ(*unsupported_session.response, (std::vector<std::uint8_t>{0x7F, 0x10, 0x12}));

    const auto unsupported_tester_present = dispatch({0x3E, 0x01}, state);
    ASSERT_TRUE(unsupported_tester_present.response.has_value());
    EXPECT_EQ(*unsupported_tester_present.response,
              (std::vector<std::uint8_t>{0x7F, 0x3E, 0x12}));

    const auto unsupported_reset = dispatch({0x11, 0x03}, state);
    ASSERT_TRUE(unsupported_reset.response.has_value());
    EXPECT_EQ(*unsupported_reset.response, (std::vector<std::uint8_t>{0x7F, 0x11, 0x12}));
}

TEST(UdsDispatcher, ReturnsIncorrectLengthForMalformedServiceRequests) {
    doip_uds::UdsConnectionState state;

    const std::vector<std::vector<std::uint8_t>> requests{
        {0x10}, {0x10, 0x01, 0x00}, {0x3E}, {0x3E, 0x00, 0x00}, {0x11}, {0x11, 0x01, 0x00}};
    for (const auto& request : requests) {
        const auto result = dispatch(request, state);
        ASSERT_TRUE(result.response.has_value());
        EXPECT_EQ((*result.response)[0], 0x7F);
        EXPECT_EQ((*result.response)[1], request[0]);
        EXPECT_EQ((*result.response)[2], doip_uds::kUdsNegativeResponseCodeIncorrectLength);
    }
}

TEST(UdsDispatcher, RejectsEmptyRequestWithoutServiceIdentifier) {
    doip_uds::UdsConnectionState state;
    EXPECT_THROW(dispatch({}, state), std::invalid_argument);
}

TEST(UdsDispatcher, ReadsConfiguredVinAndMultipleDidsInRequestOrder) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();

    const auto result =
        doip_uds::dispatch_uds_request({0x22, 0xF1, 0x90, 0xF1, 0xA0}, state, data_identifiers);

    ASSERT_TRUE(result.response.has_value());
    EXPECT_EQ(*result.response,
              (std::vector<std::uint8_t>{0x62, 0xF1, 0x90, 'W', 'V', 'W', 'Z', 'Z', 'Z',
                                         '0',  '0',  '0',  '0', '0', '0', '0', '0', '0', '0',
                                         '1',  0xF1, 0xA0, 0x00, 0x00, 0x00, 0x00}));
}

TEST(UdsDispatcher, RejectsUnknownOrUnreadableDidWithoutPartialResponse) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    data_identifiers.at(0xF1A0).readable = false;

    const auto unknown =
        doip_uds::dispatch_uds_request({0x22, 0xF1, 0x90, 0xF1, 0xA1}, state, data_identifiers);
    ASSERT_TRUE(unknown.response.has_value());
    EXPECT_EQ(*unknown.response, (std::vector<std::uint8_t>{0x7F, 0x22, 0x31}));

    const auto unreadable =
        doip_uds::dispatch_uds_request({0x22, 0xF1, 0xA0}, state, data_identifiers);
    ASSERT_TRUE(unreadable.response.has_value());
    EXPECT_EQ(*unreadable.response, (std::vector<std::uint8_t>{0x7F, 0x22, 0x31}));
}

TEST(UdsDispatcher, WritesDidOnlyInExtendedSessionAndChecksDataLength) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();

    const auto default_session =
        doip_uds::dispatch_uds_request({0x2E, 0xF1, 0xA0, 0x11, 0x22, 0x33, 0x44},
                                       state,
                                       data_identifiers);
    ASSERT_TRUE(default_session.response.has_value());
    EXPECT_EQ(*default_session.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x7E}));
    EXPECT_EQ(data_identifiers.at(0xF1A0).value,
              (std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00}));

    state.session = doip_uds::UdsDiagnosticSession::extended_session;
    const auto invalid_length =
        doip_uds::dispatch_uds_request({0x2E, 0xF1, 0xA0, 0x11, 0x22},
                                       state,
                                       data_identifiers);
    ASSERT_TRUE(invalid_length.response.has_value());
    EXPECT_EQ(*invalid_length.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x13}));

    const auto read_only =
        doip_uds::dispatch_uds_request({0x2E, 0xF1, 0x90, 0x00}, state, data_identifiers);
    ASSERT_TRUE(read_only.response.has_value());
    EXPECT_EQ(*read_only.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x31}));

    const auto written =
        doip_uds::dispatch_uds_request({0x2E, 0xF1, 0xA0, 0x11, 0x22, 0x33, 0x44},
                                       state,
                                       data_identifiers);
    ASSERT_TRUE(written.response.has_value());
    EXPECT_EQ(*written.response, (std::vector<std::uint8_t>{0x6E, 0xF1, 0xA0}));
    EXPECT_EQ(data_identifiers.at(0xF1A0).value,
              (std::vector<std::uint8_t>{0x11, 0x22, 0x33, 0x44}));
}

TEST(UdsDispatcher, SharesDidValuesAcrossConnectionSessions) {
    auto data_identifiers = make_test_data_identifiers();
    doip_uds::UdsConnectionState writer;
    writer.session = doip_uds::UdsDiagnosticSession::extended_session;
    doip_uds::UdsConnectionState reader;

    const auto written =
        doip_uds::dispatch_uds_request({0x2E, 0xF1, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4},
                                       writer,
                                       data_identifiers);
    ASSERT_TRUE(written.response.has_value());

    const auto read =
        doip_uds::dispatch_uds_request({0x22, 0xF1, 0xA0}, reader, data_identifiers);
    ASSERT_TRUE(read.response.has_value());
    EXPECT_EQ(*read.response,
              (std::vector<std::uint8_t>{0x62, 0xF1, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4}));
}

TEST(UdsDispatcher, RejectsMalformedDidRequests) {
    doip_uds::UdsConnectionState state;
    const std::vector<std::vector<std::uint8_t>> requests{
        {0x22}, {0x22, 0xF1}, {0x22, 0xF1, 0x90, 0xF1}, {0x2E}, {0x2E, 0xF1, 0xA0}};
    for (const auto& request : requests) {
        const auto result = dispatch(request, state);
        ASSERT_TRUE(result.response.has_value());
        EXPECT_EQ(*result.response,
                  (std::vector<std::uint8_t>{0x7F, request.front(), 0x13}));
    }
}

}  // namespace
