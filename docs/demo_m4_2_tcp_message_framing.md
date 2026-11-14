# demo_m4_2_tcp_message_framing

This walkthrough demonstrates the stream framing and payload-size limit
described in the [m4_2_tcp_message_framing](m4_2_tcp_message_framing.md). The server
logs complete messages but does not yet process their payloads or send DoIP
responses.

## Run the demo

Build the Debug server if it is not already built:

```powershell
cmake --preset debug
cmake --build --preset debug
```

Start the server in one PowerShell terminal:

```powershell
.\build\debug\doip_server.exe
```

In another terminal, run the demo client:

```powershell
.\tools\doip_m4_2_framing_demo.ps1
```

Use `-Destination` and `-Port` to target a different server address or port.

## What to observe

The script sends one routing-activation-shaped DoIP message in two TCP writes,
then sends two diagnostic-shaped messages in one write. TCP may combine or
split writes; the server should still log three complete frames with payload
types `0x0005` and `0x8001`. These are sample framed messages only: routing
activation and diagnostic handling are not implemented yet, so no protocol
responses are expected.

Next, the script opens a separate connection and sends a generic header
declaring a payload of 16 MiB + 1 byte. It sends only the header, not the
declared payload. The server should log that the declared payload exceeds the
configured maximum and close that client connection. The server itself should
remain available.

The 16 MiB limit is an implementation memory-safety limit, not a DoIP standard
limit. See the [framing guide](m4_2_tcp_message_framing.md) for buffering behavior,
error handling, and unit-test coverage.
