# m4_3_routing_activation

M4.3 adds Routing Activation Request (`0x0005`) decoding, Routing Activation
Response (`0x0006`) encoding, and routing-activation state on each TCP
connection. It implements the version-0x02 routing-activation format and
selected request policies; this milestone does not claim complete DoIP
conformance.

## Payloads

The request contains the tester logical address, activation type, four ISO
reserved bytes, and an optional four-byte OEM field. The supported request
payload sizes are 7 and 11 bytes. The response contains the tester address,
DoIP entity address, response code, four zeroed ISO-reserved bytes, and an
optional four-byte OEM field; its supported sizes are 9 and 13 bytes.

The codec preserves optional OEM data when encoding and decoding payloads.
Reserved bytes are zeroed when encoding and currently ignored when decoding.
The server does not implement OEM-specific behavior and omits the optional OEM
field in its responses.

## Server policy

- Only Default activation (`0x00`) is supported. Other activation types receive
  response code `0x06` (unsupported activation type).
- Tester logical addresses in the external-test-equipment range
  `0x0E00`–`0x0FFF` are accepted; other addresses receive `0x00` (unknown
  source address).
- A successful activation receives response code `0x10` and records the
  tester address on that connection.
- A tester address already active on another connection receives `0x03`.
  Multiple clients remain supported when they use distinct tester addresses.
- An activated connection requesting a different tester address receives
  `0x02`; repeating activation with the same address remains successful.
- Invalid routing-activation payload sizes or unsupported DoIP protocol
  versions close only the affected TCP connection.

Routing activation state is released when its TCP connection closes. M4.4
will use that state to gate diagnostic messages and send ACK/NACK responses.

## Tests and demo

Unit tests cover the request and response codecs, selected address and
activation policy, duplicate tester addresses, and per-connection state. A
TCP loopback test sends requests over a socket and verifies success and
rejection responses. Run the focused tests from the repository root:

```powershell
cmake --build --preset debug
.\build\debug\tests\unit_tests.exe --gtest_filter="RoutingActivation*.*"
.\build\debug\tests\integration_tests.exe --gtest_filter="RoutingActivationTcp.*"
```

See the separate [M4.3 routing activation demo](demo_m4_3_routing_activation.md)
for a walkthrough of success and rejection responses over TCP.
