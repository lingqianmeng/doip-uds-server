# DoIP discovery demo — M4.1 TCP connections

This is an M4.1 variant of the [UDP discovery demo](DOIP_DISCOVERY_DEMO.md).
It leaves that original demo unchanged and adds visible TCP connection
traffic alongside its UDP VAM/VIR exchange.

## What changes from the original demo

| Step | M3 demo | M4.1 variant |
|---|---|---|
| Capture filter | `udp port 13400` | `port 13400` to capture both TCP and UDP |
| Capture file | `captures\doip_udp.pcapng` | `captures\doip_m4_1.pcapng`, so the original capture is not overwritten |
| Server command | `.\build\debug\doip_server.exe` | Unchanged |
| Terminal 3 | Sends a VIR and receives a VAM | One combined client performs VIR/VAM, then opens a TCP connection |

Use the prerequisites and Loopback interface selection in the
[original demo guide](DOIP_DISCOVERY_DEMO.md). Run these commands from the
repository root in three PowerShell terminals.

## Terminal 1 — capture both protocols

Verify the current Loopback interface number with `dumpcap -D`, then start a
new capture. The example uses interface 9:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "port 13400" -w captures\doip_m4_1.pcapng
```

The capture filter `port 13400` matches TCP and UDP traffic. Keep this
terminal running while you start the server and run the demo client.

## Terminal 2 — start the server (unchanged)

Use the same server command as the original demo:

```powershell
.\build\debug\doip_server.exe
```

The server continues to send its startup VAMs and answer UDP discovery. It
also listens for TCP connections on port 13400 and logs accepted and
disconnected clients.

## Terminal 3 — run the complete VIR/VAM and TCP demo

Run the M4.1 client. It performs the UDP VIR/VAM exchange, prints the ECU
identity, then establishes a TCP connection and holds it open:

```powershell
.\tools\doip_m4_1_demo.ps1 -Destination 127.0.0.1 -HoldSeconds 10
```

The original [M3 UDP client](../tools/doip_vir_vam_demo.ps1) remains available
and unchanged for the original demo. The server should log `[VAM response]`,
`[TCP] Accepted client`, and, after the hold period, `[TCP] Client ...
disconnected`.

## Demonstrate concurrent TCP clients

Keep the server and capture running. To prove that multiple connections are
open at the same time, use two PowerShell terminals for TCP clients. Start the
first connection in Terminal 3 and have it stay open for 20 seconds:

```powershell
.\tools\doip_tcp_connect_demo.ps1 -Destination 127.0.0.1 -HoldSeconds 20
```

While Terminal 3 reports that it is holding the connection, run the same
command in Terminal 4:

```powershell
.\tools\doip_tcp_connect_demo.ps1 -Destination 127.0.0.1 -HoldSeconds 20
```

For three or more simultaneous clients, open another terminal for each client
and run the command before the earlier clients' hold periods expire. In the
server terminal, verify that each client has its own `[TCP] Accepted client`
line, with a different source port, before any corresponding
`[TCP] Client ... disconnected` line appears. The clients then disconnect
independently when their hold periods end.

In Wireshark, keep the display filter `tcp.port == 13400` to inspect the TCP
handshakes, or use `udp.port == 13400 || tcp.port == 13400` to include the UDP
VIR/VAM traffic. The concurrent clients should appear as separate TCP streams.

After the combined demo and any concurrent-client tests finish, stop `dumpcap`
with **Ctrl+C**, then stop the server with **Ctrl+C** in Terminal 2.

## Inspect the capture

Open the separate M4.1 capture file:

```powershell
wireshark .\captures\doip_m4_1.pcapng
```

Use this display filter to show the UDP discovery and TCP connection:

```text
udp.port == 13400 || tcp.port == 13400
```

You should see the UDP startup VAMs and VIR/VAM exchange, plus the TCP
three-way handshake and connection close. The TCP connection carries **no
DoIP messages yet**: routing activation is planned for M4.3. To focus only on
the new TCP activity, use `tcp.port == 13400`.
