#include "doip_uds/ecu_identity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

int hex_value(char character);

bool required_boolean(const Json& config, const char* name) {
    const Json& field = required_field(config, name);
    if (!field.is_boolean()) {
        throw std::runtime_error(std::string("Configuration field '") + name + "' must be a boolean");
    }
    return field.get<bool>();
}

std::uint16_t parse_did_identifier(const Json& field) {
    if (!field.is_string()) {
        throw std::runtime_error("DID configuration field 'identifier' must be a string");
    }
    const auto value = field.get<std::string>();
    if (value.size() != 6 || value[0] != '0' ||
        (value[1] != 'x' && value[1] != 'X')) {
        throw std::runtime_error(
            "DID configuration field 'identifier' must be a 16-bit hexadecimal string such as 0xF1A0");
    }

    std::uint16_t identifier = 0;
    for (std::size_t index = 2; index < value.size(); ++index) {
        const int digit = hex_value(value[index]);
        if (digit < 0) {
            throw std::runtime_error(
                "DID configuration field 'identifier' must be a 16-bit hexadecimal string such as 0xF1A0");
        }
        identifier =
            static_cast<std::uint16_t>((identifier << 4U) | static_cast<std::uint16_t>(digit));
    }
    return identifier;
}

std::vector<std::uint8_t> parse_did_value(const Json& field) {
    if (!field.is_array() || field.empty()) {
        throw std::runtime_error(
            "DID configuration field 'initial_value' must be a non-empty array of bytes");
    }

    std::vector<std::uint8_t> value;
    value.reserve(field.size());
    for (const auto& byte : field) {
        if (byte.is_number_unsigned()) {
            const auto number = byte.get<std::uint64_t>();
            if (number > 0xFFU) {
                throw std::runtime_error(
                    "DID configuration field 'initial_value' bytes must be between 0 and 255");
            }
            value.push_back(static_cast<std::uint8_t>(number));
        } else if (byte.is_number_integer()) {
            const auto number = byte.get<std::int64_t>();
            if (number < 0 || number > 0xFF) {
                throw std::runtime_error(
                    "DID configuration field 'initial_value' bytes must be between 0 and 255");
            }
            value.push_back(static_cast<std::uint8_t>(number));
        } else {
            throw std::runtime_error(
                "DID configuration field 'initial_value' must contain only integer bytes");
        }
    }
    return value;
}

std::vector<EcuDidConfiguration> parse_dids(const Json& config) {
    const auto field = config.find("dids");
    if (field == config.end()) {
        return {};
    }
    if (!field->is_array()) {
        throw std::runtime_error("Configuration field 'dids' must be an array");
    }

    std::vector<EcuDidConfiguration> dids;
    std::set<std::uint16_t> identifiers;
    dids.reserve(field->size());
    for (const auto& did : *field) {
        if (!did.is_object()) {
            throw std::runtime_error("Each entry in configuration field 'dids' must be an object");
        }
        const auto identifier = parse_did_identifier(required_field(did, "identifier"));
        if (identifier == kVinDataIdentifier) {
            throw std::runtime_error(
                "Configuration field 'dids' cannot override the built-in VIN DID 0xF190");
        }
        if (!identifiers.insert(identifier).second) {
            throw std::runtime_error("Configuration field 'dids' contains a duplicate identifier");
        }

        auto initial_value = parse_did_value(required_field(did, "initial_value"));
        const bool readable = required_boolean(did, "readable");
        const bool writable = required_boolean(did, "writable");
        if (!readable && !writable) {
            throw std::runtime_error(
                "Each configured DID must be readable, writable, or both");
        }
        dids.push_back({identifier, std::move(initial_value), readable, writable});
    }
    return dids;
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
    const auto dids = parse_dids(config);
    return EcuIdentity{vin, logical_address, eid, gid, dids};
}

}  // namespace doip_uds
