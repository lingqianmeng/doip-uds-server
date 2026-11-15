# demo_m4_5_tcp_diagnostic_flow

This walkthrough demonstrates the complete M4.5 TCP flow on a single
connection: Default routing activation, a Diagnostic Message, and its positive
acknowledgement. It has a dedicated client script and capture file.

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
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m4_5.pcapng
```

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the M4.5 client:**

```powershell
.\tools\doip_m4_5_tcp_diagnostic_flow_demo.ps1 -Destination 127.0.0.1
```

The client sends a Default Routing Activation Request for tester `0x0E00`,
checks the success response, sends UDS bytes `22 F1 90` to ECU `0x1001`, and
checks the diagnostic ACK before closing. Stop `dumpcap` and the server with
**Ctrl+C** after the script finishes.

## What to observe

On the same TCP stream, expect:

| Exchange | Expected payload type | Expected result |
|---|---:|---|
| Routing Activation Response | `0x0006` | Tester `0x0E00`, ECU `0x1001`, code `0x10` |
| Diagnostic Message | `0x8001` | Source `0x0E00`, target `0x1001`, UDS bytes `22 F1 90` |
| Diagnostic Message ACK | `0x8002` | Source `0x1001`, target `0x0E00`, code `0x00` |

The ACK confirms that the DoIP transport accepted the diagnostic bytes. No UDS
service response is expected because service handling is a later milestone.

Open the separate M4.5 capture:

```powershell
wireshark .\captures\doip_m4_5.pcapng
```

Use the display filter `tcp.port == 13400`. TCP packet boundaries may not
match DoIP frame boundaries; use **Follow → TCP Stream** to inspect the
reassembled activation and diagnostic exchange.
