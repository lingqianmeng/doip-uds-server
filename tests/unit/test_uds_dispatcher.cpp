#include "doip_uds/uds_dispatcher.hpp"

#include <stdexcept>
#include <string>
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

doip_uds::UdsDtcStore make_test_dtcs() {
    doip_uds::UdsDtcStore dtcs;
    for (const auto& definition : doip_uds::kDeploymentDtcDefinitions) {
        dtcs.push_back(
            {std::string(definition.symbolic_name), definition.pseudo_dfcc, 0x09});
    }
    return dtcs;
}

doip_uds::UdsDispatchResult dispatch(const std::vector<std::uint8_t>& request,
                                    doip_uds::UdsConnectionState& state) {
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    return doip_uds::dispatch_uds_request(request, state, data_identifiers, dtcs);
}

doip_uds::UdsDispatchResult dispatch(const std::vector<std::uint8_t>& request,
                                    doip_uds::UdsConnectionState& state,
                                    doip_uds::UdsDataIdentifierStore& data_identifiers,
                                    doip_uds::UdsDtcStore& dtcs) {
    return doip_uds::dispatch_uds_request(request, state, data_identifiers, dtcs);
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
    EXPECT_EQ(*unsupported_session.response,
              (std::vector<std::uint8_t>{0x50, 0x02, 0x00, 0x32, 0x01, 0xF4}));
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::programming_session);

    const auto unsupported_fota_session = dispatch({0x10, 0x05}, state);
    ASSERT_TRUE(unsupported_fota_session.response.has_value());
    EXPECT_EQ(*unsupported_fota_session.response,
              (std::vector<std::uint8_t>{0x7F, 0x10, 0x12}));

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
    auto dtcs = make_test_dtcs();

    const auto result =
        dispatch({0x22, 0xF1, 0x90, 0xF1, 0xA0}, state, data_identifiers, dtcs);

    ASSERT_TRUE(result.response.has_value());
    EXPECT_EQ(*result.response,
              (std::vector<std::uint8_t>{0x62, 0xF1, 0x90, 'W', 'V', 'W', 'Z', 'Z', 'Z',
                                         '0',  '0',  '0',  '0', '0', '0', '0', '0', '0', '0',
                                         '1',  0xF1, 0xA0, 0x00, 0x00, 0x00, 0x00}));
}

TEST(UdsDispatcher, RejectsUnknownOrUnreadableDidWithoutPartialResponse) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    data_identifiers.at(0xF1A0).readable = false;

    const auto unknown =
        dispatch({0x22, 0xF1, 0x90, 0xF1, 0xA1}, state, data_identifiers, dtcs);
    ASSERT_TRUE(unknown.response.has_value());
    EXPECT_EQ(*unknown.response, (std::vector<std::uint8_t>{0x7F, 0x22, 0x31}));

    const auto unreadable =
        dispatch({0x22, 0xF1, 0xA0}, state, data_identifiers, dtcs);
    ASSERT_TRUE(unreadable.response.has_value());
    EXPECT_EQ(*unreadable.response, (std::vector<std::uint8_t>{0x7F, 0x22, 0x31}));
}

TEST(UdsDispatcher, WritesDidOnlyInExtendedSessionAndChecksDataLength) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();

    const auto default_session =
        dispatch({0x2E, 0xF1, 0xA0, 0x11, 0x22, 0x33, 0x44}, state, data_identifiers, dtcs);
    ASSERT_TRUE(default_session.response.has_value());
    EXPECT_EQ(*default_session.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x7E}));
    EXPECT_EQ(data_identifiers.at(0xF1A0).value,
              (std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00}));

    state.session = doip_uds::UdsDiagnosticSession::extended_session;
    const auto invalid_length =
        dispatch({0x2E, 0xF1, 0xA0, 0x11, 0x22}, state, data_identifiers, dtcs);
    ASSERT_TRUE(invalid_length.response.has_value());
    EXPECT_EQ(*invalid_length.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x13}));

    const auto read_only =
        dispatch({0x2E, 0xF1, 0x90, 0x00}, state, data_identifiers, dtcs);
    ASSERT_TRUE(read_only.response.has_value());
    EXPECT_EQ(*read_only.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x31}));

    const auto locked =
        dispatch({0x2E, 0xF1, 0xA0, 0x11, 0x22, 0x33, 0x44}, state, data_identifiers, dtcs);
    ASSERT_TRUE(locked.response.has_value());
    EXPECT_EQ(*locked.response, (std::vector<std::uint8_t>{0x7F, 0x2E, 0x33}));
    EXPECT_EQ(data_identifiers.at(0xF1A0).value,
              (std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00}));

    const auto seed =
        dispatch({0x27, 0x5F}, state, data_identifiers, dtcs);
    ASSERT_TRUE(seed.response.has_value());
    EXPECT_EQ(*seed.response,
              (std::vector<std::uint8_t>{0x67, 0x5F, 0x00, 0x00, 0x12, 0x34}));

    const auto unlocked =
        dispatch({0x27, 0x60, 0x00, 0x00, 0xB7, 0xF7},
                 state,
                 data_identifiers,
                 dtcs);
    ASSERT_TRUE(unlocked.response.has_value());
    EXPECT_EQ(*unlocked.response, (std::vector<std::uint8_t>{0x67, 0x60}));
    EXPECT_TRUE(state.security_unlocked);

    const auto written =
        dispatch({0x2E, 0xF1, 0xA0, 0x11, 0x22, 0x33, 0x44}, state, data_identifiers, dtcs);
    ASSERT_TRUE(written.response.has_value());
    EXPECT_EQ(*written.response, (std::vector<std::uint8_t>{0x6E, 0xF1, 0xA0}));
    EXPECT_EQ(data_identifiers.at(0xF1A0).value,
              (std::vector<std::uint8_t>{0x11, 0x22, 0x33, 0x44}));
}

TEST(UdsDispatcher, SharesDidValuesAcrossConnectionSessions) {
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    doip_uds::UdsConnectionState writer;
    writer.session = doip_uds::UdsDiagnosticSession::extended_session;
    doip_uds::UdsConnectionState reader;

    const auto seed =
        doip_uds::dispatch_uds_request({0x27, 0x5F}, writer, data_identifiers, dtcs);
    ASSERT_TRUE(seed.response.has_value());
    const auto key =
        doip_uds::dispatch_uds_request({0x27, 0x60, 0x00, 0x00, 0xB7, 0xF7},
                                       writer,
                                       data_identifiers,
                                       dtcs);
    ASSERT_TRUE(key.response.has_value());

    const auto written =
        doip_uds::dispatch_uds_request({0x2E, 0xF1, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4},
                                       writer,
                                       data_identifiers,
                                       dtcs);
    ASSERT_TRUE(written.response.has_value());

    const auto read =
        doip_uds::dispatch_uds_request({0x22, 0xF1, 0xA0}, reader, data_identifiers, dtcs);
    ASSERT_TRUE(read.response.has_value());
    EXPECT_EQ(*read.response,
              (std::vector<std::uint8_t>{0x62, 0xF1, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4}));
    EXPECT_FALSE(reader.security_unlocked);
}

TEST(UdsDispatcher, HandlesSecurityAccessSequenceAndRejectsInvalidKeys) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();

    const auto default_session =
        dispatch({0x27, 0x11}, state, data_identifiers, dtcs);
    ASSERT_TRUE(default_session.response.has_value());
    EXPECT_EQ(*default_session.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x22}));

    state.session = doip_uds::UdsDiagnosticSession::extended_session;
    const auto key_without_seed =
        dispatch({0x27, 0x60, 0x00, 0x00, 0xB7, 0xF7},
                 state,
                 data_identifiers,
                 dtcs);
    ASSERT_TRUE(key_without_seed.response.has_value());
    EXPECT_EQ(*key_without_seed.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x24}));

    const auto seed = dispatch({0x27, 0x5F}, state, data_identifiers, dtcs);
    ASSERT_TRUE(seed.response.has_value());
    EXPECT_EQ(*seed.response,
              (std::vector<std::uint8_t>{0x67, 0x5F, 0x00, 0x00, 0x12, 0x34}));

    const auto invalid_key =
        dispatch({0x27, 0x60, 0x00, 0x00, 0x00, 0x00},
                 state,
                 data_identifiers,
                 dtcs);
    ASSERT_TRUE(invalid_key.response.has_value());
    EXPECT_EQ(*invalid_key.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x35}));
    EXPECT_FALSE(state.security_unlocked);

    const auto consumed_seed =
        dispatch({0x27, 0x60, 0x00, 0x00, 0xB7, 0xF7},
                 state,
                 data_identifiers,
                 dtcs);
    ASSERT_TRUE(consumed_seed.response.has_value());
    EXPECT_EQ(*consumed_seed.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x24}));

    const auto next_seed = dispatch({0x27, 0x5F}, state, data_identifiers, dtcs);
    ASSERT_TRUE(next_seed.response.has_value());
    EXPECT_EQ(*next_seed.response,
              (std::vector<std::uint8_t>{0x67, 0x5F, 0x00, 0x00, 0x13, 0x35}));

    const auto correct_key =
        dispatch({0x27, 0x60, 0x00, 0x00, 0xB6, 0xF6},
                 state,
                 data_identifiers,
                 dtcs);
    ASSERT_TRUE(correct_key.response.has_value());
    EXPECT_EQ(*correct_key.response, (std::vector<std::uint8_t>{0x67, 0x60}));
    EXPECT_TRUE(state.security_unlocked);

    const auto already_unlocked_seed = dispatch({0x27, 0x5F}, state, data_identifiers, dtcs);
    ASSERT_TRUE(already_unlocked_seed.response.has_value());
    EXPECT_EQ(*already_unlocked_seed.response,
              (std::vector<std::uint8_t>{0x67, 0x5F, 0x00, 0x00, 0x00, 0x00}));
    EXPECT_TRUE(state.security_unlocked);
}

TEST(UdsDispatcher, SupportsBootloaderSecurityAccessInProgrammingSession) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();

    const auto wrong_session = dispatch({0x27, 0x11}, state, data_identifiers, dtcs);
    ASSERT_TRUE(wrong_session.response.has_value());
    EXPECT_EQ(*wrong_session.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x22}));

    const auto programming_session = dispatch({0x10, 0x02}, state, data_identifiers, dtcs);
    ASSERT_TRUE(programming_session.response.has_value());
    EXPECT_EQ(*programming_session.response,
              (std::vector<std::uint8_t>{0x50, 0x02, 0x00, 0x32, 0x01, 0xF4}));

    const auto seed = dispatch({0x27, 0x11}, state, data_identifiers, dtcs);
    ASSERT_TRUE(seed.response.has_value());
    EXPECT_EQ(*seed.response,
              (std::vector<std::uint8_t>{0x67, 0x11, 0x00, 0x00, 0x12, 0x34}));

    const auto key =
        dispatch({0x27, 0x12, 0x00, 0x00, 0xB7, 0xF7}, state, data_identifiers, dtcs);
    ASSERT_TRUE(key.response.has_value());
    EXPECT_EQ(*key.response, (std::vector<std::uint8_t>{0x67, 0x12}));
    EXPECT_TRUE(state.security_unlocked);
}

TEST(UdsDispatcher, ResetsSecurityAccessOnSessionChangeAndEcuReset) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    state.session = doip_uds::UdsDiagnosticSession::extended_session;
    const auto seed = dispatch({0x27, 0x5F}, state, data_identifiers, dtcs);
    ASSERT_TRUE(seed.response.has_value());

    const auto returned_to_default =
        dispatch({0x10, 0x01}, state, data_identifiers, dtcs);
    ASSERT_TRUE(returned_to_default.response.has_value());
    EXPECT_FALSE(state.security_unlocked);

    const auto reentered_extended =
        dispatch({0x10, 0x03}, state, data_identifiers, dtcs);
    ASSERT_TRUE(reentered_extended.response.has_value());
    const auto stale_key =
        dispatch({0x27, 0x60, 0x00, 0x00, 0xB7, 0xF7},
                 state,
                 data_identifiers,
                 dtcs);
    ASSERT_TRUE(stale_key.response.has_value());
    EXPECT_EQ(*stale_key.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x24}));

    state.security_unlocked = true;
    const auto reset = dispatch({0x11, 0x01}, state, data_identifiers, dtcs);
    ASSERT_TRUE(reset.response.has_value());
    EXPECT_FALSE(state.security_unlocked);
    EXPECT_EQ(state.session, doip_uds::UdsDiagnosticSession::default_session);
}

TEST(UdsDispatcher, RejectsMalformedAndUnsupportedSecurityAccessRequests) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    state.session = doip_uds::UdsDiagnosticSession::extended_session;

    const std::vector<std::vector<std::uint8_t>> malformed_requests{
        {0x27}, {0x27, 0x5F, 0x00}, {0x27, 0x60}, {0x27, 0x60, 0x12, 0x34, 0x56}};
    for (const auto& request : malformed_requests) {
        const auto result = dispatch(request, state, data_identifiers, dtcs);
        ASSERT_TRUE(result.response.has_value());
        EXPECT_EQ(*result.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x13}));
    }

    const auto unsupported =
        dispatch({0x27, 0x01}, state, data_identifiers, dtcs);
    ASSERT_TRUE(unsupported.response.has_value());
    EXPECT_EQ(*unsupported.response, (std::vector<std::uint8_t>{0x7F, 0x27, 0x12}));
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

TEST(UdsDispatcher, ReportsDtcCountAndRecordsByStatusMask) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();

    const auto count =
        doip_uds::dispatch_uds_request({0x19, 0x01, 0x09}, state, data_identifiers, dtcs);
    ASSERT_TRUE(count.response.has_value());
    EXPECT_EQ(*count.response,
              (std::vector<std::uint8_t>{0x59, 0x01, 0xFF, 0x01, 0x00, 0x0A}));

    const auto records =
        doip_uds::dispatch_uds_request({0x19, 0x02, 0x08}, state, data_identifiers, dtcs);
    ASSERT_TRUE(records.response.has_value());
    EXPECT_EQ(*records.response,
              (std::vector<std::uint8_t>{
                  0x59, 0x02, 0xFF,
                  0xF0, 0x00, 0x01, 0x09,
                  0xF0, 0x00, 0x02, 0x09,
                  0xF0, 0x00, 0x03, 0x09,
                  0xF0, 0x00, 0x04, 0x09,
                  0xF0, 0x00, 0x05, 0x09,
                  0xF0, 0x00, 0x06, 0x09,
                  0xF0, 0x00, 0x07, 0x09,
                  0xF0, 0x00, 0x08, 0x09,
                  0xF0, 0x00, 0x09, 0x09,
                  0xF0, 0x00, 0x0A, 0x09}));
}

TEST(UdsDispatcher, FiltersDtcResultsByAnyRequestedStatusBit) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    dtcs[0].status = 0x01;
    dtcs[1].status = 0x08;
    dtcs[2].status = 0x00;

    const auto count =
        doip_uds::dispatch_uds_request({0x19, 0x01, 0x08}, state, data_identifiers, dtcs);
    ASSERT_TRUE(count.response.has_value());
    EXPECT_EQ(*count.response,
              (std::vector<std::uint8_t>{0x59, 0x01, 0xFF, 0x01, 0x00, 0x08}));

    const auto records =
        doip_uds::dispatch_uds_request({0x19, 0x02, 0x08}, state, data_identifiers, dtcs);
    ASSERT_TRUE(records.response.has_value());
    EXPECT_EQ(*records.response, (std::vector<std::uint8_t>{0x59, 0x02, 0xFF, 0xF0, 0x00,
                                                            0x02, 0x08, 0xF0, 0x00, 0x04,
                                                            0x09, 0xF0, 0x00, 0x05, 0x09,
                                                            0xF0, 0x00, 0x06, 0x09, 0xF0,
                                                            0x00, 0x07, 0x09, 0xF0, 0x00,
                                                            0x08, 0x09, 0xF0, 0x00, 0x09,
                                                            0x09, 0xF0, 0x00, 0x0A, 0x09}));
}

TEST(UdsDispatcher, SuppressesPositiveReadDtcResponse) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();

    const auto result =
        doip_uds::dispatch_uds_request({0x19, 0x81, 0xFF}, state, data_identifiers, dtcs);

    EXPECT_FALSE(result.response.has_value());
}

TEST(UdsDispatcher, ClearsAllDtcsOnlyInExtendedSession) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();

    const auto default_session =
        doip_uds::dispatch_uds_request({0x14, 0xFF, 0xFF, 0xFF},
                                       state,
                                       data_identifiers,
                                       dtcs);
    ASSERT_TRUE(default_session.response.has_value());
    EXPECT_EQ(*default_session.response, (std::vector<std::uint8_t>{0x7F, 0x14, 0x7E}));
    EXPECT_EQ(dtcs.size(), 10U);

    state.session = doip_uds::UdsDiagnosticSession::extended_session;
    const auto unsupported_group =
        doip_uds::dispatch_uds_request({0x14, 0x00, 0x00, 0x01},
                                       state,
                                       data_identifiers,
                                       dtcs);
    ASSERT_TRUE(unsupported_group.response.has_value());
    EXPECT_EQ(*unsupported_group.response, (std::vector<std::uint8_t>{0x7F, 0x14, 0x31}));
    EXPECT_EQ(dtcs.size(), 10U);

    const auto cleared =
        doip_uds::dispatch_uds_request({0x14, 0xFF, 0xFF, 0xFF},
                                       state,
                                       data_identifiers,
                                       dtcs);
    ASSERT_TRUE(cleared.response.has_value());
    EXPECT_EQ(*cleared.response, (std::vector<std::uint8_t>{0x54}));
    EXPECT_TRUE(dtcs.empty());

    const auto empty_count =
        doip_uds::dispatch_uds_request({0x19, 0x01, 0xFF}, state, data_identifiers, dtcs);
    ASSERT_TRUE(empty_count.response.has_value());
    EXPECT_EQ(*empty_count.response,
              (std::vector<std::uint8_t>{0x59, 0x01, 0xFF, 0x01, 0x00, 0x00}));
}

TEST(UdsDispatcher, ReturnsNegativeResponsesForMalformedDtcRequests) {
    doip_uds::UdsConnectionState state;
    auto data_identifiers = make_test_data_identifiers();
    auto dtcs = make_test_dtcs();
    const std::vector<std::vector<std::uint8_t>> malformed_requests{
        {0x19}, {0x19, 0x01}, {0x19, 0x01, 0xFF, 0x00}, {0x14},
        {0x14, 0xFF, 0xFF}, {0x14, 0xFF, 0xFF, 0xFF, 0x00}};
    for (const auto& request : malformed_requests) {
        const auto result =
            doip_uds::dispatch_uds_request(request, state, data_identifiers, dtcs);
        ASSERT_TRUE(result.response.has_value());
        EXPECT_EQ(*result.response,
                  (std::vector<std::uint8_t>{0x7F, request.front(), 0x13}));
    }

    const auto unsupported_subfunction =
        doip_uds::dispatch_uds_request({0x19, 0x03, 0xFF}, state, data_identifiers, dtcs);
    ASSERT_TRUE(unsupported_subfunction.response.has_value());
    EXPECT_EQ(*unsupported_subfunction.response,
              (std::vector<std::uint8_t>{0x7F, 0x19, 0x12}));
}

}  // namespace
