#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "doip_uds/ecu_identity.hpp"
#include "doip_uds/doip_header.hpp"
#include "doip_uds/diagnostic_message.hpp"
#include "doip_uds/routing_activation.hpp"
#include "doip_uds/tcp_transport.hpp"
#include "doip_uds/udp_transport.hpp"
#include "doip_uds/uds_dispatcher.hpp"
#include "doip_uds/version.hpp"
#include "doip_uds/vehicle_identification_server.hpp"

namespace {

volatile std::sig_atomic_t stop_requested = 0;

struct ServerOptions {
    std::string config_path{"config/ecu_config.json"};
    std::string bind_address{"0.0.0.0"};
    std::uint16_t tcp_port{doip_uds::kDoipTcpPort};
    std::uint16_t udp_port{doip_uds::kDoipUdpPort};
    bool show_help{};
    bool config_path_set{};
};

void handle_interrupt(int) {
    stop_requested = 1;
}

std::uint16_t parse_port(const std::string& value, std::string_view option_name) {
    unsigned int parsed_port{};
    const auto result =
        std::from_chars(value.data(), value.data() + value.size(), parsed_port);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        parsed_port > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument(std::string(option_name) + " must be between 0 and 65535");
    }
    return static_cast<std::uint16_t>(parsed_port);
}

ServerOptions parse_server_options(int argc, char* argv[]) {
    ServerOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        const auto next_value = [&]() -> std::string {
            if (index + 1 >= argc) {
                throw std::invalid_argument(argument + " requires a value");
            }
            return argv[++index];
        };

        if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else if (argument == "--config") {
            if (options.config_path_set) {
                throw std::invalid_argument("configuration path was specified more than once");
            }
            options.config_path = next_value();
            options.config_path_set = true;
        } else if (argument == "--bind-address") {
            options.bind_address = next_value();
        } else if (argument == "--tcp-port") {
            options.tcp_port = parse_port(next_value(), "--tcp-port");
        } else if (argument == "--udp-port") {
            options.udp_port = parse_port(next_value(), "--udp-port");
        } else if (!argument.empty() && argument.front() != '-') {
            if (options.config_path_set) {
                throw std::invalid_argument("configuration path was specified more than once");
            }
            options.config_path = argument;
            options.config_path_set = true;
        } else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    return options;
}

void print_usage(std::ostream& output) {
    output << "Usage: doip_server [config-path | --config PATH] [--bind-address ADDRESS] "
              "[--tcp-port PORT] [--udp-port PORT]\n"
              "Ports may be 0 to let the operating system choose an available port.\n";
}

struct TcpClientSession {
    doip_uds::TcpConnection connection;
    doip_uds::RoutingActivationSession routing_activation;
    doip_uds::UdsConnectionState uds_state;
    bool closing{false};
};

void process_tcp_message(TcpClientSession& client,
                         const std::vector<TcpClientSession>& clients,
                         const doip_uds::EcuIdentity& identity,
                         doip_uds::UdsDataIdentifierStore& data_identifiers,
                         doip_uds::UdsDtcStore& dtcs,
                         const std::vector<std::uint8_t>& message) {
    const auto header = doip_uds::decode_doip_header(message.data(), message.size());
    if (!header) {
        throw std::runtime_error("framed message has an invalid generic header");
    }

    std::cout << "[TCP] Received DoIP frame from " << client.connection.peer_address() << ':'
              << client.connection.peer_port() << " type=0x" << std::hex << std::uppercase
              << std::setw(4) << header->payload_type << std::dec << " bytes=" << message.size()
              << '\n';

    if (header->payload_type == doip_uds::kDoipPayloadTypeRoutingActivationRequest) {
        if (header->protocol_version != doip_uds::kDoipProtocolVersion) {
            throw std::runtime_error("routing activation requires DoIP protocol version 0x02");
        }

        const auto request = doip_uds::decode_routing_activation_request(
            message.data() + doip_uds::kDoipGenericHeaderSize,
            static_cast<std::size_t>(header->payload_length));
        if (!request) {
            throw std::runtime_error("routing activation request has an invalid payload length");
        }

        const bool active_on_another_socket =
            std::any_of(clients.begin(), clients.end(), [&client, &request](const auto& other) {
                return &other != &client && !other.closing &&
                       other.routing_activation.active_tester_logical_address() ==
                           request->tester_logical_address;
            });
        const auto response = client.routing_activation.handle_request(
            *request, identity.logical_address, active_on_another_socket);
        const auto response_payload = doip_uds::encode_routing_activation_response(response);
        client.connection.send_message(doip_uds::kDoipPayloadTypeRoutingActivationResponse,
                                       response_payload);

        std::cout << "[TCP] Sent routing activation response to "
                  << client.connection.peer_address() << ':' << client.connection.peer_port()
                  << " tester=0x" << std::hex << std::uppercase << std::setw(4)
                  << response.tester_logical_address << " entity=0x" << std::setw(4)
                  << response.entity_logical_address << " code=0x" << std::setw(2)
                  << static_cast<unsigned int>(response.response_code) << std::dec << '\n';
        return;
    }

    if (header->payload_type != doip_uds::kDoipPayloadTypeDiagnosticMessage) {
        return;
    }
    if (header->protocol_version != doip_uds::kDoipProtocolVersion) {
        throw std::runtime_error("diagnostic message requires DoIP protocol version 0x02");
    }

    const auto result = doip_uds::handle_diagnostic_message(
        message.data() + doip_uds::kDoipGenericHeaderSize,
        static_cast<std::size_t>(header->payload_length),
        client.routing_activation.active_tester_logical_address(),
        identity.logical_address);
    client.connection.send_message(result.response_payload_type, result.response_payload);

    if (result.accepted_message) {
        std::cout << "[TCP] Accepted diagnostic message from tester=0x" << std::hex
                  << std::uppercase << std::setw(4)
                  << result.accepted_message->source_logical_address << " target=0x"
                  << std::setw(4) << result.accepted_message->target_logical_address << std::dec
                  << " uds_bytes=" << result.accepted_message->user_data.size()
                  << " ack_code=0x" << std::hex << std::uppercase << std::setw(2)
                  << static_cast<unsigned int>(result.response_code) << std::dec << '\n';

        std::vector<std::string> cleared_dtc_names;
        const auto& uds_request = result.accepted_message->user_data;
        if (!uds_request.empty() &&
            uds_request.front() == doip_uds::kUdsServiceClearDiagnosticInformation) {
            cleared_dtc_names.reserve(dtcs.size());
            for (const auto& dtc : dtcs) {
                cleared_dtc_names.push_back(dtc.symbolic_name);
            }
        }
        const auto uds_result = doip_uds::dispatch_uds_request(
            uds_request, client.uds_state, data_identifiers, dtcs);
        if (!cleared_dtc_names.empty() && uds_result.response &&
            *uds_result.response == std::vector<std::uint8_t>{0x54}) {
            std::cout << "[UDS] Cleared synthetic DTCs:";
            for (const auto& name : cleared_dtc_names) {
                std::cout << ' ' << name;
            }
            std::cout << '\n';
        }
        if (uds_result.reset_requested == doip_uds::UdsEcuResetType::hard_reset) {
            std::cout << "[UDS] Simulated ECU hard reset; session returned to default\n";
        }
        if (uds_result.response) {
            const doip_uds::DiagnosticMessage uds_response{
                identity.logical_address,
                result.accepted_message->source_logical_address,
                *uds_result.response};
            client.connection.send_message(
                doip_uds::kDoipPayloadTypeDiagnosticMessage,
                doip_uds::encode_diagnostic_message(uds_response));
            std::cout << "[UDS] Sent response SID=0x" << std::hex << std::uppercase
                      << std::setw(2) << static_cast<unsigned int>(uds_result.response->front())
                      << " to tester=0x" << std::setw(4)
                      << result.accepted_message->source_logical_address << std::dec << '\n';
        } else {
            std::cout << "[UDS] Positive response suppressed\n";
        }
    } else {
        std::cout << "[TCP] Sent diagnostic NACK to " << client.connection.peer_address() << ':'
                  << client.connection.peer_port() << " code=0x" << std::hex << std::uppercase
                  << std::setw(2) << static_cast<unsigned int>(result.response_code) << std::dec
                  << '\n';
    }
}

void accept_tcp_connections(doip_uds::TcpListener& listener,
                            const doip_uds::EcuIdentity& identity,
                            doip_uds::UdsDataIdentifierStore& data_identifiers,
                            doip_uds::UdsDtcStore& dtcs,
                            const std::atomic<bool>& tcp_failed) {
    std::vector<TcpClientSession> connections;
    constexpr auto kAcceptTimeout = std::chrono::milliseconds(100);

    while (stop_requested == 0 && !tcp_failed.load()) {
        auto connection = listener.accept_for(kAcceptTimeout);
        if (connection) {
            std::cout << "[TCP] Accepted client " << connection->peer_address() << ':'
                      << connection->peer_port() << '\n';
            connections.push_back({std::move(*connection), {}, {}, false});
        }

        for (auto& client : connections) {
            if (client.closing) {
                continue;
            }
            try {
                if (!client.connection.is_connected()) {
                    std::cout << "[TCP] Client " << client.connection.peer_address() << ':'
                              << client.connection.peer_port() << " disconnected\n";
                    client.closing = true;
                    continue;
                }

                constexpr std::size_t kMaxFramesPerClientPerIteration = 32;
                for (std::size_t frame_index = 0;
                     frame_index < kMaxFramesPerClientPerIteration;
                     ++frame_index) {
                    const auto message =
                        client.connection.receive_message_for(std::chrono::milliseconds::zero());
                    if (!message) {
                        break;
                    }
                    process_tcp_message(
                        client, connections, identity, data_identifiers, dtcs, *message);
                }

                if (!client.connection.is_connected()) {
                    std::cout << "[TCP] Client " << client.connection.peer_address() << ':'
                              << client.connection.peer_port() << " disconnected\n";
                    client.closing = true;
                }
            } catch (const std::exception& error) {
                std::cerr << "[TCP] Closing client " << client.connection.peer_address() << ':'
                          << client.connection.peer_port() << ": " << error.what() << '\n';
                client.closing = true;
            }
        }

        connections.erase(std::remove_if(connections.begin(),
                                         connections.end(),
                                         [](const TcpClientSession& client) {
                                             return client.closing;
                                         }),
                          connections.end());
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    ServerOptions options;
    try {
        options = parse_server_options(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Invalid command line: " << error.what() << '\n';
        print_usage(std::cerr);
        return 1;
    }
    if (options.show_help) {
        print_usage(std::cout);
        return 0;
    }

    doip_uds::EcuIdentity identity;
    doip_uds::UdsDataIdentifierStore data_identifiers;
    doip_uds::UdsDtcStore dtcs;
    try {
        identity = doip_uds::load_ecu_identity(options.config_path);
        data_identifiers = doip_uds::make_uds_data_identifier_store(identity);
        dtcs = doip_uds::make_uds_dtc_store(identity);
        std::cout << "doip_server " << doip_uds::version()
                  << " loaded ECU identity at logical address 0x" << std::hex << std::uppercase
                  << std::setw(4) << std::setfill('0') << identity.logical_address << '\n';
        if (!identity.dtcs.empty()) {
            std::cout << "[UDS] Configured synthetic DTCs:";
            for (const auto& dtc : identity.dtcs) {
                std::cout << ' ' << dtc.symbolic_name;
            }
            std::cout << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to load ECU identity: " << error.what() << '\n';
        return 1;
    }

    try {
        doip_uds::UdpListener listener(options.bind_address, options.udp_port);
        doip_uds::TcpListener tcp_listener(options.bind_address, options.tcp_port);
        std::signal(SIGINT, handle_interrupt);
        std::cout << std::dec
                  << "Listening for DoIP vehicle-identification requests on UDP port "
                  << listener.port() << " and TCP connections on port " << tcp_listener.port()
                  << ". Press Ctrl+C to stop.\n"
                  << std::flush;
        doip_uds::send_startup_vehicle_announcements(listener, identity);

        std::exception_ptr tcp_error;
        std::atomic<bool> tcp_failed{false};
        std::thread tcp_acceptor([&] {
            try {
                accept_tcp_connections(tcp_listener, identity, data_identifiers, dtcs, tcp_failed);
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
