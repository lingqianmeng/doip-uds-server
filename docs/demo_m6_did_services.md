# demo_m6_did_services

This walkthrough demonstrates ReadDataByIdentifier (`0x22`) and
WriteDataByIdentifier (`0x2E`). It uses the sample `config/ecu_config.json`,
whose default VIN is read through DID `0xF190` and whose configured writable
simulation DID is `0xF1A0`.

## Run the demo

Run the commands from the repository root. Build the Debug server if needed:

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
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m6.pcapng
```

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the M6 client:**

```powershell
.\tools\doip_m6_did_services_demo.ps1 -Destination 127.0.0.1
```

The client activates routing, reads the VIN and writable DID in default
session, confirms that a default-session write is denied, enters extended
session, and confirms that the DID write remains locked. It performs the
simulator-only SecurityAccess (`0x27`) seed/key exchange, writes `11 22 33 44`
to DID `0xF1A0`, and reads the updated value back. It also verifies that the
read-only VIN DID cannot be written. Leave the capture running briefly after
the client exits to record the TCP teardown, then stop `dumpcap` and the server
with **Ctrl+C**.

## Expected UDS exchanges

Each request first receives a DoIP Diagnostic Message ACK (`0x8002`); each UDS
response then arrives in a separate reverse-addressed Diagnostic Message
(`0x8001`).

| Request | Expected UDS response |
|---|---|
| `22 F1 90 F1 A0` | `62 F1 90` + 17 ASCII VIN bytes + `F1 A0 00 00 00 00` |
| `2E F1 A0 11 22 33 44` in default session | `7F 2E 7E` |
| `10 03` | `50 03 00 32 01 F4` |
| `2E F1 A0 11 22 33 44` in extended session before unlock | `7F 2E 33` |
| `27 01` | `67 01 12 34` |
| `27 02 B7 F7` | `67 02` |
| `2E F1 A0 11 22 33 44` after unlock | `6E F1 A0` |
| `22 F1 A0 F1 90` | `62 F1 A0 11 22 33 44 F1 90` + 17 ASCII VIN bytes |
| `2E F1 90 00` in extended session | `7F 2E 31` |

Writes update process memory only. Restarting the server restores the initial
configured DID value. The seed/key exchange is a deterministic simulator
demonstration, not a production security mechanism; see the
[M8 SecurityAccess guide](m8_security_access.md).

Open the capture with:

```powershell
wireshark .\captures\doip_m6.pcapng
```

Use the display filter `tcp.port == 13400`. TCP packet boundaries are not
DoIP frame boundaries; use **Follow → TCP Stream** to inspect the separate
DoIP ACKs and UDS responses.
