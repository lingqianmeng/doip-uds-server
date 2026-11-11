#include <chrono>
#include <csignal>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/udp_transport.hpp"
#include "doip_uds/version.hpp"
#include "doip_uds/vehicle_identification_server.hpp"

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void handle_interrupt(int) {
    stop_requested = 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: doip_server [config-path]\n";
        return 1;
    }

    const std::string config_path = argc == 2 ? argv[1] : "config/ecu_config.json";
    doip_uds::EcuIdentity identity;
    try {
        identity = doip_uds::load_ecu_identity(config_path);
        std::cout << "doip_server " << doip_uds::version()
                  << " loaded ECU identity at logical address 0x" << std::hex << std::uppercase
                  << std::setw(4) << std::setfill('0') << identity.logical_address << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Failed to load ECU identity: " << error.what() << '\n';
        return 1;
    }

    try {
        doip_uds::UdpListener listener;
        std::signal(SIGINT, handle_interrupt);
        std::cout << "Listening for DoIP vehicle-identification requests on UDP port "
                  << listener.port() << ". Press Ctrl+C to stop.\n";

        constexpr auto kReceiveTimeout = std::chrono::milliseconds(100);
        while (stop_requested == 0) {
            doip_uds::process_next_vehicle_identification_request(
                listener, identity, kReceiveTimeout);
        }
        std::cout << "\nDoIP server stopped.\n";
    } catch (const std::exception& error) {
        std::cerr << "DoIP server failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
