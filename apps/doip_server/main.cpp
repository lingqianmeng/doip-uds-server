#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/version.hpp"

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: doip_server [config-path]\n";
        return 1;
    }

    const std::string config_path = argc == 2 ? argv[1] : "config/ecu_config.json";
    try {
        const auto identity = doip_uds::load_ecu_identity(config_path);
        std::cout << "doip_server " << doip_uds::version()
                  << " loaded ECU identity at logical address 0x" << std::hex << std::uppercase
                  << std::setw(4) << std::setfill('0') << identity.logical_address << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Failed to load ECU identity: " << error.what() << '\n';
        return 1;
    }

    // DoIP UDP/TCP listeners follow in milestones 3-4.
    return 0;
}
