# demo_m5_uds_services

This walkthrough demonstrates M5 SessionControl (`0x10`), TesterPresent
(`0x3E`), and simulated ECUReset (`0x11`) after DoIP routing activation. Each
request is sent on the same TCP connection. The capture shows the separation
between DoIP transport ACKs and UDS service responses.

## Run the demo

Run the commands from the repository root. Build the Debug server if needed:

```powershell
cmake --preset debug
cmake --build --preset debug
```

Use three PowerShell terminals. Start the capture before the server and find
the current `NPF_Loopback` interface number with `dumpcap -D`.

**Terminal 1 — capture the full TCP exchange on port 13400:**

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m5.pcapng
```

Replace interface number `9` if `NPF_Loopback` has a different number.

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the M5 client:**

```powershell
.\tools\doip_m5_uds_services_demo.ps1 -Destination 127.0.0.1
```

The client activates routing, enters the extended session, sends normal and
response-suppressed TesterPresent requests, and requests a simulated hard ECU
reset. The reset is logged and returns the simulated session to default; it
does not restart the server. Leave the capture running briefly after the client
exits to include the TCP disconnection, then stop `dumpcap` and the server with
**Ctrl+C**.

## What to observe

Use the Wireshark display filter `tcp.port == 13400`. On the same TCP stream,
expect the DoIP Diagnostic Message ACK (`0x8002`) before each UDS response,
which is carried in a reverse-addressed Diagnostic Message (`0x8001`):

| UDS request | DoIP exchange | UDS result |
|---|---|---|
| SessionControl extended `10 03` | ACK, then Diagnostic Message | `50 03 00 32 01 F4` |
| TesterPresent `3E 00` | ACK, then Diagnostic Message | `7E 00` |
| TesterPresent suppressed `3E 80` | ACK only | No positive UDS response |
| ECUReset hard reset `11 01` | ACK, then Diagnostic Message | `51 01`; simulated reset, no process restart |

Open the saved capture with:

```powershell
wireshark .\captures\doip_m5.pcapng
```

TCP packet boundaries are not DoIP message boundaries; use **Follow → TCP
Stream** to inspect the reassembled request/response sequence and final TCP
teardown.
