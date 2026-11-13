# TCP transport (M4.1)

M4.1 adds a cross-platform IPv4 TCP listener on the standard DoIP port
(`13400`) alongside the existing UDP discovery listener. TCP and UDP have
separate port namespaces, so the server can bind both transports to port 13400
at the same time.

## Connection lifecycle

`doip_uds::TcpListener` binds and listens for TCP clients. Its bounded
`accept_for()` call returns a move-only `TcpConnection` that owns the accepted
socket and records the peer IPv4 address and source port. An accepted
connection remains open until the peer disconnects or the connection object
is destroyed. `is_connected()` checks for peer closure without consuming any
incoming bytes, leaving stream parsing to M4.2. If a peer closes after sending
bytes, those queued bytes must be read before the close can be detected; M4.1
deliberately leaves them unread because stream framing is M4.2.

The listener polls with a 100 ms interval. Calling `shutdown()` causes a
waiting `accept_for()` to return within approximately one poll interval. Socket
setup, accept, and connection-state errors are reported as
`std::system_error`; invalid bind addresses and negative timeouts are rejected
with `std::invalid_argument`.

## Client/session policy

The server accepts and tracks multiple concurrent TCP clients. Each accepted
socket has an independent `TcpConnection` lifetime; when a disconnect is
observable, the server logs and releases that connection without dropping the
others. M4.1 does not consume any received bytes, so a close behind queued data
is not observable until that data is read. This milestone only establishes and
manages TCP connections. It does not process DoIP messages, perform routing
activation, or mark a connection as diagnostically active. Those behaviors
are introduced by M4.2–M4.4.

The executable keeps UDP discovery processing on its main thread and accepts
TCP clients on a separate thread. Ctrl+C stops the UDP loop and TCP accept
loop, joins the worker, and releases active TCP connections.

To observe both UDP discovery and the TCP handshake without changing the
original UDP-only walkthrough, use the separate
[M4.1 demo variant](DOIP_DISCOVERY_DEMO_M4_1.md).

## Tests

`tests/unit/test_tcp_transport.cpp` uses loopback sockets to verify ephemeral
binding and accept, timeout behavior, cooperative shutdown, invalid arguments,
and two simultaneous clients with independent disconnect lifetimes. Run the
focused tests from the repository root:

```powershell
cmake --build --preset debug
.\build\debug\tests\unit_tests.exe --gtest_filter="TcpListener.*"
```

The full suite can be run with `ctest --preset debug`.
