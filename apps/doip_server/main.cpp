#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/tcp_transport.hpp"
#include "doip_uds/udp_transport.hpp"
#include "doip_uds/version.hpp"
#include "doip_uds/vehicle_identification_server.hpp"

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void handle_interrupt(int) {
    stop_requested = 1;
}

void accept_tcp_connections(doip_uds::TcpListener& listener,
                            const std::atomic<bool>& tcp_failed) {
    std::vector<doip_uds::TcpConnection> connections;
    constexpr auto kAcceptTimeout = std::chrono::milliseconds(100);

    while (stop_requested == 0 && !tcp_failed.load()) {
        auto connection = listener.accept_for(kAcceptTimeout);
        if (connection) {
            std::cout << "[TCP] Accepted client " << connection->peer_address() << ':'
                      << connection->peer_port() << '\n';
            connections.push_back(std::move(*connection));
        }

        connections.erase(
            std::remove_if(connections.begin(),
                           connections.end(),
                           [](const doip_uds::TcpConnection& active_connection) {
                               if (active_connection.is_connected()) {
                                   return false;
                               }
                               std::cout << "[TCP] Client " << active_connection.peer_address()
                                         << ':' << active_connection.peer_port()
                                         << " disconnected\n";
                               return true;
                           }),
            connections.end());
    }
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
        doip_uds::TcpListener tcp_listener;
        std::signal(SIGINT, handle_interrupt);
        std::cout << std::dec
                  << "Listening for DoIP vehicle-identification requests on UDP port "
                  << listener.port() << " and TCP connections on port " << tcp_listener.port()
                  << ". Press Ctrl+C to stop.\n";
        doip_uds::send_startup_vehicle_announcements(listener, identity);

        std::exception_ptr tcp_error;
        std::atomic<bool> tcp_failed{false};
        std::thread tcp_acceptor([&] {
            try {
                accept_tcp_connections(tcp_listener, tcp_failed);
            } catch (...) {
                tcp_error = std::current_exception();
                tcp_failed.store(true);
            }
        });

        constexpr auto kReceiveTimeout = std::chrono::milliseconds(100);
        try {
            while (stop_requested == 0 && !tcp_failed.load()) {
                doip_uds::process_next_vehicle_identification_request(
                    listener, identity, kReceiveTimeout);
            }
        } catch (...) {
            stop_requested = 1;
            tcp_listener.shutdown();
            tcp_acceptor.join();
            throw;
        }
        tcp_listener.shutdown();
        tcp_acceptor.join();
        if (tcp_error) {
            std::rethrow_exception(tcp_error);
        }
        std::cout << "\nDoIP server stopped.\n";
    } catch (const std::exception& error) {
        std::cerr << "DoIP server failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
