# demo_m4_5_tcp_diagnostic_flow

This walkthrough captures the broader DoIP activity: startup vehicle
announcements, a vehicle-identification request and response (VIR/VAM), then
the M4.5 TCP flow on one connection: Default routing activation, a Diagnostic
Message, its positive DoIP acknowledgement, and the M5 UDS response.

## Run the demo

Run the commands from the repository root. Build the Debug server if it is not
already built:

```powershell
cmake --preset debug
cmake --build --preset debug
```

The capture walkthrough uses Scoop-installed Wireshark and Npcap, including
the Npcap Loopback adapter. Use three PowerShell terminals.

**Terminal 1 — capture all DoIP traffic on port 13400:**

Check the current interface numbers with `dumpcap -D` and find the
`NPF_Loopback` interface. Replace the example interface number if needed:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "port 13400" -w captures\doip_m4_5.pcapng
```

The capture filter `port 13400` includes both UDP and TCP, including the TCP
connection handshake and DoIP messages. Start capturing before the server to
include its startup VAMs.

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — perform discovery, then the M4.5 TCP flow:**

First send a VIR to the local server and receive its VAM response:

```powershell
.\tools\doip_vir_vam_demo.ps1 -Destination 127.0.0.1
```

Then run the M4.5 TCP client:

```powershell
.\tools\doip_m4_5_tcp_diagnostic_flow_demo.ps1 -Destination 127.0.0.1
```

The TCP client sends a Default Routing Activation Request for tester
`0x0E00`, checks the success response, sends UDS bytes `22 F1 90` to ECU
`0x1001`, then checks the DoIP ACK and the separate UDS negative response
`7F 22 11`. Service `0x22` is not implemented yet. Leave the capture running
briefly after the client exits to record the TCP disconnection, then stop
`dumpcap` and the server with **Ctrl+C**.

## What to observe in the capture

Use the Wireshark display filter `udp.port == 13400 || tcp.port == 13400` to
show both transports. The trace should include the following:

| Exchange | Transport / payload type | Expected result |
|---|---|---|
| Startup VAMs | UDP / `0x0004` | Three vehicle announcements from server startup |
| Demo VIR | UDP / `0x0001` | Broadcast-format request sent to `127.0.0.1` |
| VIR response | UDP / `0x0004` | Unicast VAM with the configured ECU identity |
| TCP connection | TCP | Handshake on port 13400 |
| Routing Activation Request | TCP / `0x0005` | Tester `0x0E00`, Default activation |
| Routing Activation Response | TCP / `0x0006` | Tester `0x0E00`, ECU `0x1001`, code `0x10` |
| Diagnostic Message | TCP / `0x8001` | Source `0x0E00`, target `0x1001`, UDS bytes `22 F1 90` |
| Diagnostic Message ACK | TCP / `0x8002` | Source `0x1001`, target `0x0E00`, code `0x00` |
| UDS response | TCP / `0x8001` | Source `0x1001`, target `0x0E00`, bytes `7F 22 11` (service unsupported) |
| TCP disconnection | TCP | Graceful connection teardown after the UDS response (FIN/ACK packets) |

The DoIP ACK confirms transport acceptance. The M5 dispatcher then returns
`7F 22 11` because UDS service `0x22` is not supported.

Open the M4.5 capture:

```powershell
wireshark .\captures\doip_m4_5.pcapng
```

The combined display filter can be narrowed to either transport. TCP packet
boundaries may not match DoIP frame boundaries; use **Follow → TCP Stream** to
inspect the reassembled activation and diagnostic exchange.
