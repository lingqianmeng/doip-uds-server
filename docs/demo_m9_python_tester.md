# demo_m9_python_tester

This walkthrough runs the Python tester against the local simulated ECU. It
uses only Python's standard library and exercises DoIP routing activation and
the supported UDS diagnostic flow.

## Run the demo and capture it in Wireshark

From the repository root, build the Debug server:

```powershell
cmake --preset debug
cmake --build --preset debug
```

Use three PowerShell terminals. In **Terminal 1**, find the Npcap loopback
interface and start capturing DoIP TCP traffic. Replace `9` with the interface
number shown for `NPF_Loopback`:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m9.pcapng
```

In **Terminal 2**, start the server:

```powershell
.\build\debug\doip_server.exe
```

In **Terminal 3**, run the tester:

```powershell
python .\tools\doip_tester.py demo
```

The demo connects only to `127.0.0.1` and uses tester address `0x0E00`,
simulator address `0x1001`, and TCP port `13400` by default.
After the tester exits, stop `dumpcap` and the server with **Ctrl+C**.

Open the capture in Wireshark:

```powershell
wireshark .\captures\doip_m9.pcapng
```

Use the display filter `tcp.port == 13400`. TCP packet boundaries are not DoIP
frame boundaries; use **Follow → TCP Stream** to inspect routing activation,
diagnostic ACKs, and UDS responses.

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
