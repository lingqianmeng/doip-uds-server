# demo_m7_dtc_services

This walkthrough uses the ten symbolic synthetic DTCs in
`config/ecu_config.json`. Their wire values are simulator-only placeholders,
not production DTCs. The sample config initializes each status to `0x09`.

## Run the demo

From the repository root, build the Debug server if needed:

```powershell
cmake --preset debug
cmake --build --preset debug
```

For a loopback packet capture, use three PowerShell terminals. In Terminal 1,
find the Npcap loopback interface number with `dumpcap -D` and start a capture
(replace `9` if needed):

```powershell
& "$(scoop prefix wireshark)\dumpcap.exe" -D
& "$(scoop prefix wireshark)\dumpcap.exe" -i 9 -f "tcp port 13400" -w captures\doip_m7.pcapng
```

**Terminal 2 — start the server:**

```powershell
.\build\debug\doip_server.exe
```

**Terminal 3 — run the M7 client:**

```powershell
.\tools\doip_m7_dtc_services_demo.ps1 -Destination 127.0.0.1
```

The client activates routing, reads the ten DTCs by status mask, enters
extended session, clears group `0xFFFFFF`, and verifies that the runtime list
is empty. Each diagnostic request receives a DoIP ACK before its separate UDS
response. Stop `dumpcap` and the server with **Ctrl+C** after the demo.

## Expected UDS exchanges

| Request | Expected UDS response |
|---|---|
| `19 01 09` | `59 01 FF 01 00 0A` |
| `19 02 08` | `59 02 FF` + ten four-byte synthetic DTC/status records |
| `10 03` | `50 03 00 32 01 F4` |
| `14 FF FF FF` | `54` |
| `19 01 FF` after clear | `59 01 FF 01 00 00` |

Clear changes only process memory. Restarting the server restores the ten
configured symbolic DTCs and their initial statuses. Open the capture with:

```powershell
wireshark .\captures\doip_m7.pcapng
```

Use the display filter `tcp.port == 13400`; use **Follow → TCP Stream** to
inspect the DoIP frames and UDS payloads.
