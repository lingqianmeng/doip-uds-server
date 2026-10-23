#include <iostream>

#include "doip_uds/version.hpp"

int main() {
    std::cout << "doip_server " << doip_uds::version() << '\n';
    // DoIP UDP/TCP listeners follow in milestones 3-4.
    return 0;
}
