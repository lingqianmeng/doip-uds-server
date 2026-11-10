#include "doip_uds/vehicle_identification_server.hpp"

#include "doip_uds/vehicle_identification_handler.hpp"

namespace doip_uds {

bool process_next_vehicle_identification_request(
    UdpListener& listener,
    const EcuIdentity& identity,
    std::chrono::milliseconds receive_timeout) {
    const auto datagram = listener.receive_for(receive_timeout);
    if (!datagram) {
        return false;
    }

    const auto response =
        handle_vehicle_identification_datagram(identity, datagram->payload);
    if (response) {
        listener.send_to(datagram->sender_address, datagram->sender_port, *response);
    }
    return true;
}

}  // namespace doip_uds
