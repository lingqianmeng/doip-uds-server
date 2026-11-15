# m4_4_diagnostic_message_transport

M4.4 adds Diagnostic Message (`0x8001`) decoding, logical-address checks,
positive/negative acknowledgements, and a handoff point for future UDS
processing. It implements the version-0x02 message format and selected
single-ECU routing policy; it does not claim complete DoIP conformance.

## Message handling

A Diagnostic Message payload contains a two-byte source logical address, a
two-byte target logical address, and at least one byte of user data. The user
data is preserved as-is; this milestone does not interpret UDS services.

The server applies these checks in order:

- The TCP connection must have an active routing-activation session.
- The source address must match the tester address activated on that connection.
  Otherwise the server sends a Diagnostic Message NACK (`0x8003`) with code
  `0x02` (invalid source address).
- The target address must match the configured ECU logical address. Otherwise
  the server sends NACK code `0x03` (unknown target address).
- A valid message receives a Diagnostic Message ACK (`0x8002`) with code
  `0x00`. Its user data is returned in `accepted_message` for the caller to
  hand to a future UDS dispatcher.

ACK and NACK payloads contain the ECU source address, tester target address,
and response code. The optional previous-message data field is omitted.
Diagnostic payloads shorter than five bytes are malformed; only that TCP
connection is closed.

## Tests and demo

Unit tests cover payload encoding/decoding, inactive and mismatched source
addresses, unknown targets, and preservation of accepted user data. TCP
loopback tests verify ACK/NACK frames. Run the focused tests from the
repository root:

```powershell
cmake --build --preset debug
ctest --preset debug --output-on-failure -R "DiagnosticMessage"
```

See the separate [M4.4 diagnostic-message demo](demo_m4_4_diagnostic_message_transport.md)
for the live TCP exchange and Wireshark capture steps.
