# m4_5_tcp_diagnostic_flow

M4.5 verifies the complete TCP diagnostic flow introduced in M4.1–M4.4:
connect, activate routing, send a Diagnostic Message, and receive its
acknowledgement. The flow is covered by a loopback integration test and a
dedicated live-server demo.

## Integration coverage

The TCP integration test keeps one connection open for both Routing Activation
Request (`0x0005`) and Diagnostic Message (`0x8001`). It verifies that the
successful activation state is then used to accept the diagnostic message,
that the ACK (`0x8002`) has the expected addresses and code, and that the UDS
bytes remain unchanged. The test also verifies the M5 TesterPresent response
(`7E 00`) as a separate Diagnostic Message after the DoIP ACK. The test
composes the core handlers over a real loopback TCP connection; the separate
live-server demo exercises the executable's dispatch path.

Run the focused integration test from the repository root:

```powershell
cmake --build --preset debug
ctest --preset debug --output-on-failure -R "DiagnosticFlowTcp"
```

## Live demo

See the separate [M4.5 TCP diagnostic-flow demo](demo_m4_5_tcp_diagnostic_flow.md)
for the server, PowerShell client, and Wireshark capture steps. The demo shows
the activation response followed by the diagnostic ACK on the same connection.
The ACK confirms DoIP transport acceptance. With M5, the server follows it
with a UDS response message when a positive response is required. This demo
sends unsupported service `0x22`, so the expected UDS response is `7F 22 11`;
see the M5 demo for supported UDS services.
