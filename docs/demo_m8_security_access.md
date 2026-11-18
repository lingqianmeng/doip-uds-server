# demo_m8_security_access

This walkthrough demonstrates the simulator-only SecurityAccess (`0x27`)
exchange and its authorization of the sample writable DID `0xF1A0`.

> [!WARNING]
> The fixed XOR seed/key transform in this demo is predictable, non-cryptographic
> test logic. It is not suitable for a real ECU or production use.

## Run the demo

From the repository root, build and start the Debug server:

```powershell
cmake --preset debug
cmake --build --preset debug
.\build\debug\doip_server.exe
```

In another PowerShell terminal, run the TCP loopback client:

```powershell
.\tools\doip_m6_did_services_demo.ps1 -Destination 127.0.0.1
```

The existing M6 client also exercises M8: it enters extended session, confirms
the DID write is denied while locked, requests an AirbagDeployment seed, sends
the demonstration key, writes the DID, and reads the new value back. Every
diagnostic request receives a DoIP ACK before its separate UDS response.

## Expected SecurityAccess exchanges

For a fresh connection, the first seed is `0x00001234`. The demonstration key
is the seed XOR `0x0000A5C3`, which gives `0x0000B7F7`. Both values are sent as
four bytes in network byte order.

| Request | Expected UDS response |
|---|---|
| `10 03` | `50 03 00 32 01 F4` |
| `2E F1 A0 11 22 33 44` while locked | `7F 2E 33` |
| `27 5F` | `67 5F 00 00 12 34` |
| `27 60 00 00 B7 F7` | `67 60` |
| `2E F1 A0 11 22 33 44` after unlock | `6E F1 A0` |

Unlocking is per TCP connection and is cleared by changing diagnostic session,
ECU reset, or disconnecting. This deterministic exchange only demonstrates
UDS sequencing; it provides no real security. Bootloader access uses `27 11`
and `27 12` in programming session `0x02`. The `0x05` FOTA session and login
attempt lockout are not implemented.
