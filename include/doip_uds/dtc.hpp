#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace doip_uds {

namespace DeploymentDtc {
inline constexpr std::uint32_t RAM_TEST = 0xF00001U;
inline constexpr std::uint32_t RAM_ECC = 0xF00002U;
inline constexpr std::uint32_t CONTROL_MODULE = 0xF00003U;
inline constexpr std::uint32_t ROM_CHECK = 0xF00004U;
inline constexpr std::uint32_t MEMORY_PROTECTION = 0xF00005U;
inline constexpr std::uint32_t CONTROLLER_SUPERVISION = 0xF00006U;
inline constexpr std::uint32_t NVM_OPERATION = 0xF00007U;
inline constexpr std::uint32_t ADC_CHECK = 0xF00008U;
inline constexpr std::uint32_t POWER_MONITORING = 0xF00009U;
inline constexpr std::uint32_t SBC_CHECK = 0xF0000AU;
}  // namespace DeploymentDtc

struct DeploymentDtcDefinition {
    std::string_view symbolic_name;
    std::uint32_t pseudo_dfcc;
};

inline constexpr std::array<DeploymentDtcDefinition, 10> kDeploymentDtcDefinitions{{
    {"DTC_RAM_TEST", DeploymentDtc::RAM_TEST},
    {"DTC_RAM_ECC", DeploymentDtc::RAM_ECC},
    {"DTC_CONTROL_MODULE", DeploymentDtc::CONTROL_MODULE},
    {"DTC_ROM_CHECK", DeploymentDtc::ROM_CHECK},
    {"DTC_MEMORY_PROTECTION", DeploymentDtc::MEMORY_PROTECTION},
    {"DTC_CONTROLLER_SUPERVISION", DeploymentDtc::CONTROLLER_SUPERVISION},
    {"DTC_NVM_OPERATION", DeploymentDtc::NVM_OPERATION},
    {"DTC_ADC_CHECK", DeploymentDtc::ADC_CHECK},
    {"DTC_POWER_MONITORING", DeploymentDtc::POWER_MONITORING},
    {"DTC_SBC_CHECK", DeploymentDtc::SBC_CHECK},
}};

inline std::optional<std::uint32_t> deployment_dtc_code(std::string_view symbolic_name) {
    for (const auto& definition : kDeploymentDtcDefinitions) {
        if (definition.symbolic_name == symbolic_name) {
            return definition.pseudo_dfcc;
        }
    }
    return std::nullopt;
}

struct EcuDtcConfiguration {
    std::string symbolic_name;
    std::uint32_t pseudo_dfcc{};
    std::uint8_t initial_status{};
};

struct UdsDtcRecord {
    std::string symbolic_name;
    std::uint32_t pseudo_dfcc{};
    std::uint8_t status{};
};

using UdsDtcStore = std::vector<UdsDtcRecord>;

}  // namespace doip_uds
