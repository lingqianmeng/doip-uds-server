#include "doip_uds/ecu_identity.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

class TemporaryConfig {
public:
    explicit TemporaryConfig(const std::string& contents) {
        static std::atomic<unsigned int> counter{0};
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("doip-uds-config-" + std::to_string(unique) + "-" +
                 std::to_string(counter.fetch_add(1)) + ".json");
        std::ofstream output(path_);
        output << contents;
        if (!output) {
            throw std::runtime_error("Could not write temporary ECU configuration");
        }
    }

    ~TemporaryConfig() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::string make_config(const std::string& vin = "WVWZZZ00000000001",
                        const std::string& logical_address = "0x1001",
                        const std::string& eid = "00:00:00:00:00:01",
                        const std::string& gid = "00:00:00:00:00:02",
                        const std::string& dids = "[]") {
    return "{\"logical_address\":\"" + logical_address + "\",\"vin\":\"" + vin +
           "\",\"eid\":\"" + eid + "\",\"gid\":\"" + gid + "\",\"dids\":" + dids + "}";
}

std::string load_error(const std::string& contents) {
    const TemporaryConfig config(contents);
    try {
        static_cast<void>(doip_uds::load_ecu_identity(config.path()));
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    ADD_FAILURE() << "Expected ECU configuration loading to fail";
    return {};
}

}  // namespace

TEST(EcuIdentity, LoadsAndDecodesValidConfiguration) {
    const TemporaryConfig config(make_config());

    const auto identity = doip_uds::load_ecu_identity(config.path());

    const std::array<std::uint8_t, doip_uds::kDoipVinSize> expected_vin{
        'W', 'V', 'W', 'Z', 'Z', 'Z', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1'};
    const std::array<std::uint8_t, doip_uds::kDoipEntityIdSize> expected_eid{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    const std::array<std::uint8_t, doip_uds::kDoipEntityIdSize> expected_gid{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

    EXPECT_EQ(identity.vin, expected_vin);
    EXPECT_EQ(identity.logical_address, 0x1001);
    EXPECT_EQ(identity.eid, expected_eid);
    EXPECT_EQ(identity.gid, expected_gid);
    EXPECT_TRUE(identity.dids.empty());
}

TEST(EcuIdentity, LoadsConfiguredDataIdentifiers) {
    const TemporaryConfig config(make_config(
        "WVWZZZ00000000001",
        "0x1001",
        "00:00:00:00:00:01",
        "00:00:00:00:00:02",
        "[{\"identifier\":\"0xF1A0\",\"initial_value\":[0,1,254,255],"
        "\"readable\":true,\"writable\":true}]"));

    const auto identity = doip_uds::load_ecu_identity(config.path());

    ASSERT_EQ(identity.dids.size(), 1U);
    EXPECT_EQ(identity.dids[0].identifier, 0xF1A0);
    EXPECT_EQ(identity.dids[0].initial_value,
              (std::vector<std::uint8_t>{0x00, 0x01, 0xFE, 0xFF}));
    EXPECT_TRUE(identity.dids[0].readable);
    EXPECT_TRUE(identity.dids[0].writable);
}

TEST(EcuIdentity, RejectsInvalidDidConfiguration) {
    const std::string valid_did =
        "{\"identifier\":\"0xF1A0\",\"initial_value\":[0,0,0,0],"
        "\"readable\":true,\"writable\":true}";
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "null"))
                  .find("'dids' must be an array"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "[{}]"))
                  .find("'identifier'"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "[{\"identifier\":\"0xF190\",\"initial_value\":[1],"
                                     "\"readable\":true,\"writable\":false}]"))
                  .find("cannot override"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "[{\"identifier\":\"0xF1A0\",\"initial_value\":[],"
                                     "\"readable\":true,\"writable\":true}]"))
                  .find("non-empty array"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "[{\"identifier\":\"0xF1A0\",\"initial_value\":[256],"
                                     "\"readable\":true,\"writable\":true}]"))
                  .find("between 0 and 255"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "[" + valid_did + "," + valid_did + "]"))
                  .find("duplicate identifier"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001",
                                     "0x1001",
                                     "00:00:00:00:00:01",
                                     "00:00:00:00:00:02",
                                     "[{\"identifier\":\"0xF1A0\",\"initial_value\":[0],"
                                     "\"readable\":false,\"writable\":false}]"))
                  .find("must be readable"),
              std::string::npos);
}

TEST(EcuIdentity, ReportsMissingFieldsAndInvalidJson) {
    EXPECT_NE(load_error("{\"vin\":\"WVWZZZ00000000001\"}").find("'logical_address'"),
              std::string::npos);
    EXPECT_NE(load_error("{").find("Invalid JSON"), std::string::npos);
    EXPECT_NE(load_error("[]").find("JSON object"), std::string::npos);
    EXPECT_NE(load_error("{\"logical_address\":1001,\"vin\":\"WVWZZZ00000000001\","
                         "\"eid\":\"00:00:00:00:00:01\",\"gid\":\"00:00:00:00:00:02\"}")
                  .find("'logical_address' must be a string"),
              std::string::npos);
}

TEST(EcuIdentity, RejectsInvalidVinValues) {
    EXPECT_NE(load_error(make_config("WVWZZZ0000000000")).find("'vin'"), std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZI0000000001")).find("'vin'"), std::string::npos);
}

TEST(EcuIdentity, RejectsInvalidLogicalAddresses) {
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001", "0x0E80")).find("manufacturer range"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001", "0x0000")).find("manufacturer range"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001", "1001")).find("'logical_address'"),
              std::string::npos);
}

TEST(EcuIdentity, RejectsMalformedOrInvalidEntityIdentifiers) {
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001", "0x1001", "00-00-00-00-00-01"))
                  .find("'eid'"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001", "0x1001", "00:00:00:00:00:00"))
                  .find("invalidity value"),
              std::string::npos);
    EXPECT_NE(load_error(make_config("WVWZZZ00000000001", "0x1001", "00:00:00:00:00:01",
                                     "FF:FF:FF:FF:FF:FF"))
                  .find("invalidity value"),
              std::string::npos);
}

TEST(EcuIdentity, ReportsConfigurationFileOpenFailure) {
    const auto missing_path = std::filesystem::temp_directory_path() /
                              ("doip-uds-missing-config-" +
                               std::to_string(std::chrono::steady_clock::now()
                                                  .time_since_epoch()
                                                  .count()) +
                               ".json");
    try {
        static_cast<void>(doip_uds::load_ecu_identity(missing_path));
        FAIL() << "Expected a missing configuration file to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("Unable to open"), std::string::npos);
    }
}
