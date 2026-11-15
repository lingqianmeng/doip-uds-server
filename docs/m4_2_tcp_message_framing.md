# m4_2_tcp_message_framing

TCP is a byte stream, so a `send()` call does not correspond to one `recv()`
call. A DoIP generic header or payload may arrive in pieces, and several DoIP
messages may arrive together. M4.2 adds per-connection buffering so the server
can extract complete messages using the DoIP generic header's payload length.

## Framing behavior

`TcpConnection::receive_message_for(timeout)` buffers bytes until it has:

1. The complete 8-byte DoIP generic header.
2. The complete payload declared by that header.

It validates the generic header's protocol-version inverse byte, preserves
partial headers and payloads across timeouts, and returns one complete DoIP
message at a time. Any additional bytes stay buffered for later calls, so
coalesced messages are returned separately. A clean peer close with no pending
bytes returns `std::nullopt`; closing during an incomplete frame is reported as
an error. A negative timeout is rejected.

The implementation limits each TCP DoIP payload to **16 MiB**. This is a
local memory-safety limit, not a DoIP standard limit. A header declaring a
larger payload is rejected immediately with `std::length_error`; a header
with an invalid inverse byte is rejected with `std::runtime_error`. The server
logs the framing error and closes only the offending client connection.

The M4.2 demo sends generic payload frames that the server logs without
interpreting. Routing Activation Request handling and responses are covered
separately by the [M4.3 guide](m4_3_routing_activation.md) and
[M4.3 demo](demo_m4_3_routing_activation.md).

## Demo

For the Windows PowerShell walkthrough, see [demo_m4_2_tcp_message_framing](demo_m4_2_tcp_message_framing.md).

## Tests

The TCP connection tests in `tests/unit/test_tcp_transport.cpp` cover a
message split across the header and payload, multiple messages sent together,
an invalid header, a payload length above the configured limit, and peer
closure during an incomplete message.

Run the focused tests from the repository root:

```powershell
cmake --build --preset debug
.\build\debug\tests\unit_tests.exe --gtest_filter="TcpConnection.*"
```

Run the full suite with `ctest --preset debug`.
