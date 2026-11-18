# m8_security_access

**Status:** Simulator SecurityAccess behavior aligned with the supplied UDS poster.

## Goal and scope

SecurityAccess (`0x27`) supports the two poster-listed access-type pairs and uses
the resulting per-connection authorization state to protect the simulated
writable DID (`0xF1A0`). The DoIP transport and its ACK-before-UDS-response
ordering are unchanged.

> [!WARNING]
> This seed/key transform is deterministic demonstration logic for this
> simulator only. It is not cryptographic, must not be used to protect a real
> ECU, and must not be copied as a production security algorithm.

## Boundary and state

- Support requestSeed/sendKey access types `0x11`/`0x12` for Bootloader and
  `0x5F`/`0x60` for AirbagDeployment.
- Require the programming session (`0x02`) for Bootloader and the extended
  session (`0x03`) for AirbagDeployment. SessionControl also supports `0x02`;
  FOTA session `0x05` remains unsupported. A successful exchange authorizes
  configured writable DID writes for that TCP connection while in extended
  session. The Bootloader pair can be exercised in programming session, but
  there are no programming services that consume its authorization.
- Generate deterministic, per-connection 32-bit demonstration seeds. The first
  seed is `0x00001234`; each subsequent request advances it by `0x0101`. This
  is predictable by design and is not a security-strength random source.
- For demonstration only, the expected 32-bit key is the seed XOR `0x0000A5C3`.
  For example, seed `0x00001234` expects key `0x0000B7F7`.
- Keep the pending seed and unlocked state per connection. A wrong key consumes
  the pending seed. A new connection, diagnostic-session transition, or ECU
  reset starts locked. Once unlocked, another requestSeed returns a four-byte
  zero seed and leaves access unlocked.
- No attempt counter, delay timer, persistent credentials, or real ECU key
  derivation is implemented; NRCs `0x36` and `0x37` are therefore not generated.

## Request and response behavior

All multibyte values use network byte order.

| Request | Positive response | Behavior |
|---|---|---|
| `10 02`, then `27 11` | `50 02 ...`, then `67 11 00 00 12 34` | Enters programming session and creates a Bootloader seed. |
| `27 12 00 00 B7 F7` | `67 12` | Completes the Bootloader exchange; no programming service currently uses this authorization. |
| `10 03`, then `27 5F` | `50 03 ...`, then `67 5F 00 00 12 34` | Enters extended session and creates an AirbagDeployment seed. |
| `27 60 00 00 B7 F7` | `67 60` | Unlocks the simulator's writable-DID authorization in extended session. |
| `2E <DID> <data>` in extended session while locked | `7F 2E 33` | Does not modify DID data. |
| `2E <DID> <data>` in extended session after unlock | `6E <DID>` | Writes the configured writable DID. |

Malformed lengths return NRC `0x13`, unsupported access types return `0x12`,
using a supported type outside its required session returns `0x22`, sending a
key without its matching pending seed returns `0x24`, and an incorrect key
returns `0x35`. A locked DID write returns `0x33`. Existing write validation
still returns `0x31` for unknown or non-writable DIDs and `0x7E` for a write
outside extended session.

Session state and authorization are connection-local. Runtime DID values
remain process-shared and are reset to configuration on server restart.

## Verification and live demo

Unit tests cover both poster-listed access-type pairs, session restrictions,
request sequencing, seed progression, correct and incorrect keys, malformed
requests, write authorization, per-connection state, and authorization reset.
The TCP integration test verifies the AirbagDeployment seed/key exchange and
guarded DID write within the existing DoIP diagnostic flow.

Run the Debug tests:

```powershell
cmake --build --preset debug
ctest --preset debug --output-on-failure -R "UdsDispatcher|DiagnosticFlowTcp"
```

The [M8 live demo](demo_m8_security_access.md) captures the full flow. The
simulator algorithm is intentionally unsuitable for production.
