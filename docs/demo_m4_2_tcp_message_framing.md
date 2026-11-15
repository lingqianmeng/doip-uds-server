# demo_m4_2_tcp_message_framing

This walkthrough demonstrates the stream framing and payload-size limit
described in the [m4_2_tcp_message_framing](m4_2_tcp_message_framing.md). It
uses generic payload type `0x9000` frames to focus on TCP framing without
mixing in behavior from later milestones.

## Run the demo

Run the commands from the repository root. Build the Debug server if it is not
already built:

```powershell
cmake --preset debug
cmake --build --preset debug
```

The capture walkthrough uses Scoop-installed Wireshark and Npcap, including
the Npcap Loopback adapter.

Use three PowerShell terminals.

**Terminal 1 — capture TCP port 13400:**

Check the current interface numbers with `dumpcap -D` and find the
`NPF_Loopback` interface. The example uses interface 9; replace it if the
interface number differs on your machine:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m4_2.pcapng
```

Leave the capture running.

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the demo client:**

```powershell
.\tools\doip_m4_2_framing_demo.ps1
```

Use `-Destination` and `-Port` to target a different server address or port.
After the demo finishes, stop `dumpcap` with **Ctrl+C** in Terminal 1 and stop
the server with **Ctrl+C** in Terminal 2.

## What to observe

The script sends one generic DoIP frame in two TCP writes, then sends two
complete generic frames in one write. The server should log all three frames
with payload type `0x9000` and send no protocol responses. TCP may combine or
split the client's writes, but the DoIP frames should still be assembled
correctly.

Next, the script opens a separate connection and sends a generic header
declaring a payload of 16 MiB + 1 byte. It sends only the header, not the
declared payload. The server should log that the declared payload exceeds the
configured maximum and close that client connection. The server itself should
remain available.

The 16 MiB limit is an implementation memory-safety limit, not a DoIP standard
limit. See the [framing guide](m4_2_tcp_message_framing.md) for buffering behavior,
error handling, and unit-test coverage.

## Inspect the capture

Open the saved trace in Wireshark:

```powershell
wireshark .\captures\doip_m4_2.pcapng
```

Use the display filter:

```text
tcp.port == 13400
```

The trace includes the TCP connections and message bytes sent by the demo.
Inspect packet data for the generic headers: `02 FD 90 00` for the three
generic frames and `02 FD 90 00 01 00 00 01` for the oversized
length declaration. TCP packet boundaries may not match the demo's write
boundaries; use **Follow → TCP Stream** to view the connection's reassembled
byte stream.
