# DoIP UDP discovery demo

This guide demonstrates the server's startup vehicle-announcement messages
(VAMs) and a vehicle-identification request (VIR) followed by its VAM
response. It uses Scoop-installed Wireshark's `dumpcap.exe` to capture UDP
port 13400 on the Npcap Loopback adapter.

## Prerequisites

- Build the project in Debug mode:

  ```powershell
  cmake --preset debug
  cmake --build --preset debug
  ```

- Install Wireshark with Scoop and make sure Npcap is installed and available
  to your user. The `dumpcap -D` output should include an `NPF_Loopback`
  adapter.
- Run all commands below from the repository root.

## Capture and run the demo

Use three PowerShell terminals. Start the capture before the server so that
the trace includes its startup VAMs.

**Terminal 1 — start `dumpcap`:**

First, verify the current interface numbers:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
```

In the interface list, identify `NPF_Loopback`. In the setup described here it
is interface 9. Start capture with:

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "udp port 13400" -w captures\doip_udp.pcapng
```

Leave this terminal running. If the interface index changes, substitute the
current `NPF_Loopback` index for `9`.

**Terminal 2 — start the DoIP server:**

```powershell
.\build\debug\doip_server.exe
```

The server loads `config\ecu_config.json` and sends three startup VAMs to
`255.255.255.255:13400`. The first is sent after a randomized delay of up to
500 ms, then the next two are sent 500 ms apart. The console labels them
`[VAM startup]` and prints the configured VIN.

**Terminal 3 — send a VIR and receive the response:**

```powershell
.\tools\doip_vir_vam_demo.ps1 -Destination 127.0.0.1
```

The client prints the VIN, logical address, EID, and GID returned by the
server. The server labels this unicast reply `[VAM response]`. The request
payload is a broadcast vehicle-identification request; this local demo sends
it to `127.0.0.1` so the request/reply exchange stays on loopback.

Stop `dumpcap` with **Ctrl+C** after the demo completes. Stop the server with
**Ctrl+C** in Terminal 2.

## Inspect the saved capture

The capture file is written to the current directory. Open it in Wireshark:

```powershell
wireshark .\captures\doip_udp.pcapng
```

Use the display filter:

```text
udp.port == 13400
```

The trace should contain three startup VAMs, followed by the demo VIR and its
VAM response. Select a packet and expand **User Datagram Protocol** and
**Data** in the packet details. If Wireshark does not decode DoIP
automatically, inspect the UDP payload bytes:

| Packet | DoIP payload type | Meaning |
|---|---:|---|
| Startup VAM | `0x0004` | Unsolicited vehicle announcement |
| Demo VIR | `0x0001` | Vehicle-identification request |
| VIR response | `0x0004` | Vehicle-identification response / VAM |

The DoIP generic header occupies UDP payload offsets 0–7. In either VAM, the
VIN is the next 17 ASCII bytes at offsets 8–24, followed by the logical
address, EID, and GID.

The capture filter used by `dumpcap` is a capture-time filter; the Wireshark
display filter above can be changed after opening the file. The interface
number `9` is specific to the current machine and should be checked with
`dumpcap -D` when adapters are added or removed.
