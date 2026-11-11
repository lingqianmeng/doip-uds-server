# UDP transport (M3.3)

M3.3 provides an IPv4 UDP listener independent of DoIP parsing, ECU identity,
and request handling. This page explains the UDP concepts behind the listener,
how the C++ API uses them, and what each transport test verifies.

## UDP in this project

UDP sends **datagrams**: each send is one independent packet containing a
bounded number of bytes. Unlike TCP, UDP does not create a connection or a
reliable byte stream. There is no built-in guarantee that a datagram arrives,
arrives only once, or arrives in the order it was sent. The application must
decide whether a packet is valid and whether to respond. In this project, the
UDP transport stays separate from DoIP framing; the M3.4 handler and M3.5
server service compose the transport with request/response logic.

A UDP socket is an operating-system endpoint. Binding it to an IPv4 address
and port reserves that local endpoint so the operating system can deliver
matching datagrams to it:

- `127.0.0.1` binds only to loopback traffic from the same machine.
- `0.0.0.0` binds to all local IPv4 interfaces, which is the default.
- Port `13400` is the standard DoIP UDP port and the API default.
- Port `0` asks the operating system to choose an available ephemeral port.
  `port()` reports the actual port assigned after binding.

Each received datagram includes its bytes and the sender's IPv4 address and
UDP source port. The server uses those sender fields to return a response to
the requesting peer.

## Listener API and lifecycle

The public interface is in `include/doip_uds/udp_transport.hpp`:

```cpp
doip_uds::UdpListener listener("0.0.0.0", doip_uds::kDoipUdpPort);
const auto datagram = listener.receive();
if (datagram) {
    // datagram->payload: bytes in this one UDP packet
    // datagram->sender_address: for example "192.0.2.10"
    // datagram->sender_port: the peer's UDP source port
    const auto response =
        handle_vehicle_identification_datagram(identity, datagram->payload);
    if (response) {
        listener.send_to(datagram->sender_address, datagram->sender_port, *response);
    }
}
```

Construction creates an IPv4 datagram socket and binds it. If setup fails,
construction throws rather than leaving a listener that looks usable but is
not. `receive()` waits for one packet, up to the maximum IPv4 UDP payload size
handled by this implementation, and returns a `UdpDatagram`. `receive_for()`
waits only up to the requested timeout and returns `std::nullopt` on timeout or
shutdown. `send_to()` sends one datagram using the listener's bound socket and
throws if the destination address is invalid or the OS reports a send error.
Socket setup, waiting, receiving, and sending errors throw `std::system_error`
with the failing operation in the message; an invalid IPv4 address throws
`std::invalid_argument`. `send_to()` rejects payloads larger than the maximum
IPv4 UDP payload size before calling the platform socket API.

The listener polls socket readiness with a 100 ms timeout. That makes shutdown
cooperative: `shutdown()` sets a thread-safe stop flag, and a waiting
`receive()` notices it within about one poll interval and returns
`std::nullopt`. Shutdown does not close a socket underneath a blocked receive.
Use one receiving thread per listener, join that thread after shutdown, and
only then destroy the listener. Destruction closes the socket and releases
Winsock resources on Windows.

The server uses the bounded `receive_for()` call with a 100 ms timeout. This
lets its main loop periodically check whether Ctrl+C requested shutdown.

## How the tests exercise it

The transport tests are in `tests/unit/test_udp_transport.cpp`. They use an
ephemeral listener port and real loopback UDP sockets, so no external network
or fixed port 13400 is required.

| Test | What it does | What it verifies |
|---|---|---|
| `BindsEphemeralPortAndReceivesDatagramWithSender` | Binds the listener to `127.0.0.1` and port `0`, sends a small byte vector to the assigned port, then receives it. The test keeps the sender socket open until after receipt. | The OS-assigned port is nonzero; packet bytes remain intact; the sender address is `127.0.0.1`; and the sender port matches the transmitting socket. |
| `ReportsAddressAlreadyInUse` | Opens one listener on an ephemeral loopback port, then attempts to bind a second listener to the same address and port. | A bind conflict is surfaced as `std::system_error` identifying the `bind` operation, instead of being silently ignored. |
| `RejectsInvalidBindAddress` | Constructs a listener using a non-IP string. | Invalid address text is rejected with `std::invalid_argument` before receiving. |
| `ShutdownInterruptsWaitingReceive` | Starts `receive()` on a worker thread with no incoming packet, calls `shutdown()`, then joins the worker. | A waiting receive exits and returns `std::nullopt`; calling `receive()` again after shutdown also returns `std::nullopt`. |
| `ReceiveForReturnsNulloptAfterTimeout` | Waits briefly on an idle listener with `receive_for()`. | A timeout returns `std::nullopt` instead of blocking indefinitely. |
| `SendToRejectsInvalidDestinationAddress` | Attempts to send using invalid IPv4 address text. | Invalid destinations throw `std::invalid_argument`. |
| `SendToRejectsOversizedDatagram` | Attempts to send more than the maximum IPv4 UDP payload size. | Oversized payloads are rejected before reaching the platform socket API. |

Run only these tests from the repository root with:

```sh
cmake --build --preset debug
./build/debug/tests/unit_tests --gtest_filter=UdpListener.*
```

On Windows PowerShell, the executable path is
`.\build\debug\tests\unit_tests.exe`. The full suite is run with
`ctest --preset debug`.

These tests demonstrate local socket behavior, not reliable delivery across a
network: they do not prove packet delivery under loss, reordering, or congestion.
UDP intentionally makes no such guarantee.

## Platform support and scope

The implementation uses Winsock on Windows and BSD/POSIX sockets on Linux and
Raspberry Pi. CMake links `ws2_32` on Windows; no third-party transport
dependency is required.

The transport remains independent of DoIP parsing. The server service composes
the listener with the M3.4 request handler: it receives a datagram, asks the
handler to validate it and build a response, then sends a valid response back
to the datagram's sender. `doip_server` runs this service on UDP port 13400.
The localhost integration test covers the complete request/response path.
