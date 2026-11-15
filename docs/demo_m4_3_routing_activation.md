# demo_m4_3_routing_activation

This walkthrough demonstrates only M4.3 routing activation: successful
activation, rejected requests, per-connection state, and duplicate tester
address handling. It has its own client script and capture file, separate
from the M4.2 TCP framing demo.

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
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m4_3.pcapng
```

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the M4.3 client:**

```powershell
.\tools\doip_m4_3_routing_activation_demo.ps1 -Destination 127.0.0.1
```

The script keeps two TCP connections open while it sends the requests, then
closes both. Stop `dumpcap` and the server with **Ctrl+C** after the script
finishes.

## What to observe

The first TCP connection activates tester `0x0E00` with Default activation.
The script keeps it open while a second connection checks duplicate-address
handling. Each request should receive:

| Connection and request | Expected response | What it verifies |
|---|---|---|
| First connection: tester `0x0E00`, activation type `0x00` | `0x10` — activated | Default activation succeeds. |
| Same connection: tester `0x0E01`, activation type `0x00` | `0x02` — different source address | An activated connection cannot switch tester addresses. |
| Same connection: tester `0x0E00`, activation type `0x01` | `0x06` — unsupported activation type | Only Default activation (`0x00`) is supported. |
| Second connection: tester `0x0E00`, activation type `0x00` | `0x03` — source address active elsewhere | The first connection still owns `0x0E00`. |
| Second connection: tester `0x0E01`, activation type `0x00` | `0x10` — activated | Separate connections may use distinct tester addresses. |
| Second connection: tester `0x0DFF`, activation type `0x00` | `0x00` — unknown source address | Addresses outside the selected tester range are rejected. |

The distinction between `0x02` and `0x03` is the connection: `0x02` rejects
changing the address on an already activated socket; `0x03` rejects reusing
another socket's active address.

In the server terminal, each request should be logged as payload type
`0x0005`, followed by a response log for payload type `0x0006`.

Open the separate M4.3 capture:

```powershell
wireshark .\captures\doip_m4_3.pcapng
```

Use the display filter `tcp.port == 13400`. The response code is the fifth
byte of each Routing Activation Response payload. TCP packet boundaries may
not align with DoIP messages; use **Follow → TCP Stream** to inspect the
reassembled connection bytes.
