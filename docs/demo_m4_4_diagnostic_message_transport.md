# demo_m4_4_diagnostic_message_transport

This walkthrough focuses on M4.4 Diagnostic Message transport: routing
activation is performed only as a prerequisite, then the script demonstrates
inactive, invalid-address, and valid diagnostic-message cases. It has its own
client script and capture file, separate from the M4.3 routing-activation demo.

## Run the demo

Run the commands from the repository root. Build the Debug server if it is not
already built:

```powershell
cmake --preset debug
cmake --build --preset debug
```

The capture walkthrough uses Scoop-installed Wireshark and Npcap, including
the Npcap Loopback adapter. Use three PowerShell terminals.

**Terminal 1 — capture TCP port 13400:**

Check the current interface numbers with `dumpcap -D` and find the
`NPF_Loopback` interface. Replace the example interface number if needed:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m4_4.pcapng
```

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the M4.4 client:**

```powershell
.\tools\doip_m4_4_diagnostic_message_demo.ps1 -Destination 127.0.0.1
```

The script uses one TCP connection throughout: it first sends a diagnostic
message before activation, performs successful Default routing activation,
then exercises address rejection and a valid message. Stop `dumpcap` and the
server with **Ctrl+C** after the script finishes.

## What to observe

| Request | Expected response | Meaning |
|---|---|---|
| Diagnostic message before activation | `0x8003`, code `0x02` | No routing-activated tester is registered on the connection. |
| Source `0x0E01` after activating `0x0E00` | `0x8003`, code `0x02` | The message source does not match the connection's tester address. |
| Target `0x1002` instead of this ECU's `0x1001` | `0x8003`, code `0x03` | The target is not the configured ECU. |
| Source `0x0E00`, target `0x1001`, UDS bytes `22 F1 90` | `0x8002`, code `0x00` | The transport accepts the message and preserves its UDS bytes without interpreting them. |

ACK/NACK response addresses are in response direction: source `0x1001` (the
ECU) and target equal to the tester address from the request. The server logs
the accepted UDS payload length; no UDS service response is expected.

Open the separate M4.4 capture:

```powershell
wireshark .\captures\doip_m4_4.pcapng
```

Use the display filter `tcp.port == 13400`. The diagnostic requests use payload
type `0x8001`; responses use `0x8002` for ACK or `0x8003` for NACK. The
response code follows the two logical addresses in the response payload. Use
**Follow → TCP Stream** to inspect the reassembled connection bytes.
