#include "doip_uds/ecu_identity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace doip_uds {
namespace {

using Json = nlohmann::json;

const Json& required_field(const Json& config, const char* name) {
    const auto field = config.find(name);
    if (field == config.end()) {
        throw std::runtime_error(std::string("Missing required configuration field '") + name + "'");
    }
    return *field;
}

std::string required_string(const Json& config, const char* name) {
    const Json& field = required_field(config, name);
    if (!field.is_string()) {
        throw std::runtime_error(std::string("Configuration field '") + name + "' must be a string");
    }
    return field.get<std::string>();
}

bool is_valid_vin_character(unsigned char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'A' && character <= 'H') ||
           (character >= 'J' && character <= 'N') ||
           character == 'P' ||
           (character >= 'R' && character <= 'Z');
}

std::array<std::uint8_t, kDoipVinSize> parse_vin(const std::string& vin) {
    if (vin.size() != kDoipVinSize ||
        !std::all_of(vin.begin(), vin.end(), [](char character) {
            return is_valid_vin_character(static_cast<unsigned char>(character));
        })) {
        throw std::runtime_error(
            "Configuration field 'vin' must contain exactly 17 uppercase VIN characters (0-9, A-H, J-N, P, R-Z)");
    }

    std::array<std::uint8_t, kDoipVinSize> result{};
    std::transform(vin.begin(), vin.end(), result.begin(), [](unsigned char character) {
        return static_cast<std::uint8_t>(character);
    });
    return result;
}

int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    const auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    if (upper >= 'A' && upper <= 'F') {
        return upper - 'A' + 10;
    }
    return -1;
}

std::array<std::uint8_t, kDoipEntityIdSize> parse_identifier(const std::string& value,
                                                             const char* name) {
    if (value.size() != 17) {
        throw std::runtime_error(std::string("Configuration field '") + name +
                                 "' must contain six colon-separated hexadecimal bytes");
    }

    std::array<std::uint8_t, kDoipEntityIdSize> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto offset = index * 3;
        if (index != result.size() - 1 && value[offset + 2] != ':') {
            throw std::runtime_error(std::string("Configuration field '") + name +
                                     "' must contain six colon-separated hexadecimal bytes");
        }
        const int high = hex_value(value[offset]);
        const int low = hex_value(value[offset + 1]);
        if (high < 0 || low < 0) {
            throw std::runtime_error(std::string("Configuration field '") + name +
                                     "' must contain six colon-separated hexadecimal bytes");
        }
        result[index] = static_cast<std::uint8_t>((high << 4) | low);
    }

    const bool all_zero = std::all_of(result.begin(), result.end(), [](std::uint8_t byte) {
        return byte == 0x00;
    });
    const bool all_ff = std::all_of(result.begin(), result.end(), [](std::uint8_t byte) {
        return byte == 0xFF;
    });
    if (all_zero || all_ff) {
        throw std::runtime_error(std::string("Configuration field '") + name +
                                 "' cannot use the all-zero or all-0xFF invalidity value");
    }
    return result;
}

std::uint16_t parse_logical_address(const std::string& value) {
    if (value.size() != 6 || value[0] != '0' ||
        (value[1] != 'x' && value[1] != 'X')) {
        throw std::runtime_error(
            "Configuration field 'logical_address' must be a 16-bit hexadecimal string such as 0x1001");
    }

    std::uint16_t address = 0;
    for (std::size_t index = 2; index < value.size(); ++index) {
        const int digit = hex_value(value[index]);
        if (digit < 0) {
            throw std::runtime_error(
                "Configuration field 'logical_address' must be a 16-bit hexadecimal string such as 0x1001");
        }
        address = static_cast<std::uint16_t>((address << 4) | static_cast<std::uint16_t>(digit));
    }

    const bool manufacturer_range = (address >= 0x0001 && address <= 0x0DFF) ||
                                    (address >= 0x1000 && address <= 0x7FFF);
    if (!manufacturer_range) {
        throw std::runtime_error(
            "Configuration field 'logical_address' must be in a vehicle-manufacturer range "
            "(0x0001-0x0DFF or 0x1000-0x7FFF)");
    }
    return address;
}

}  // namespace

EcuIdentity load_ecu_identity(const std::filesystem::path& config_path) {
    std::ifstream input(config_path);
    if (!input) {
        throw std::runtime_error("Unable to open ECU configuration file '" + config_path.string() + "'");
    }

    Json config;
    try {
        input >> config;
    } catch (const Json::parse_error& error) {
        throw std::runtime_error("Invalid JSON in ECU configuration file '" + config_path.string() +
                                 "': " + error.what());
    }

    if (!config.is_object()) {
        throw std::runtime_error("ECU configuration file must contain a JSON object");
    }

    const auto vin = parse_vin(required_string(config, "vin"));
    const auto logical_address = parse_logical_address(required_string(config, "logical_address"));
    const auto eid = parse_identifier(required_string(config, "eid"), "eid");
    const auto gid = parse_identifier(required_string(config, "gid"), "gid");
    return EcuIdentity{vin, logical_address, eid, gid};
}

}  // namespace doip_uds
