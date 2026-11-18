# demo_m9_python_tester

This walkthrough runs the Python tester against the local simulated ECU. It
uses only Python's standard library and exercises DoIP routing activation and
the supported UDS diagnostic flow.

## Run the demo

From the repository root, build and start the Debug server:

```powershell
cmake --preset debug
cmake --build --preset debug
.\build\debug\doip_server.exe
```

In another PowerShell terminal, run the tester:

```powershell
python .\tools\doip_tester.py demo
```

The demo connects only to `127.0.0.1` and uses tester address `0x0E00`,
simulator address `0x1001`, and TCP port `13400` by default.

## Expected flow

The tester activates routing and verifies each DoIP Diagnostic Message ACK
before reading its separate UDS response. It then:

- Reads the configured VIN and writable DID `0xF1A0`.
- Confirms a write is rejected in the default session and while SecurityAccess
  is locked.
- Enters extended session and performs the AirbagDeployment seed/key exchange
  using the simulator-only XOR demonstration transform.
- Writes and reads back DID `0xF1A0`, rejects a write to the read-only VIN,
  and reads the configured DTC count.

The seed/key transform is deterministic demonstration logic, not production
security. The tester has no remote-host, arbitrary UDS request, or key-guessing
mode.

The [M9 tester guide](m9_python_tester.md) describes the client protocol checks,
server port options used by integration tests, and CTest command.
