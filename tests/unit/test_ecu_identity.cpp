#include "doip_uds/ecu_identity.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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
                        const std::string& gid = "00:00:00:00:00:02") {
    return "{\"logical_address\":\"" + logical_address + "\",\"vin\":\"" + vin +
           "\",\"eid\":\"" + eid + "\",\"gid\":\"" + gid + "\"}";
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
