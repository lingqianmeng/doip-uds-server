# UDP transport (M3.3)

M3.3 adds a small IPv4 UDP listener independent of DoIP parsing and ECU
request-handling logic. `doip_uds::UdpListener` binds at construction, receives
one datagram at a time, and returns the sender's IPv4 address and port with the
exact received bytes.

```cpp
doip_uds::UdpListener listener("0.0.0.0", doip_uds::kDoipUdpPort);
const auto datagram = listener.receive();
if (datagram) {
    // datagram->payload, datagram->sender_address, datagram->sender_port
}
```

The port defaults to `kDoipUdpPort` (13400). Tests and applications can pass
port zero to ask the operating system for an available port, then query the
assigned value using `port()`. The bind address must be an IPv4 literal.

## Errors and shutdown

Socket setup, bind, readiness-wait, and receive errors throw
`std::system_error` with the failing operation in its message. An invalid
address throws `std::invalid_argument`. A listener must successfully bind
before it can be used.

`receive()` blocks while waiting for traffic but checks for shutdown at least
every 100 ms. Call `shutdown()` from another thread to make a waiting receive
return `std::nullopt`; shutdown is idempotent. A listener supports one
receiving thread. Join that thread before destroying the listener. Destruction
closes the socket and releases Winsock resources on Windows.

## Scope

This is a transport primitive only. It does not parse DoIP generic headers,
validate datagram contents, send responses, or start a listener from the
`doip_server` executable. M3.4 connects received datagrams to the header,
payload, and ECU-identity logic; M3.5 exercises the complete exchange.

The implementation uses Winsock on Windows and BSD/POSIX sockets on Linux and
Raspberry Pi. CMake links `ws2_32` on Windows; no third-party transport
dependency is required.
