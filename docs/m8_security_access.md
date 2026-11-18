# m8_security_access

**Status:** M8.1 boundary and M8.2 simulator implementation complete.

## Goal and scope

M8 adds a deliberately limited SecurityAccess (`0x27`) exchange to the UDS
dispatcher and uses the resulting per-connection authorization state to protect
the simulated writable DID (`0xF1A0`). The DoIP transport and its ACK-before-
UDS-response ordering are unchanged.

> [!WARNING]
> This seed/key transform is deterministic demonstration logic for this
> simulator only. It is not cryptographic, must not be used to protect a real
> ECU, and must not be copied as a production security algorithm.

## Boundary and state

- Support requestSeed subfunction `0x01` and sendKey subfunction `0x02` for one
  simulator security level.
- Require the extended diagnostic session for both subfunctions. A successful
  `0x27` exchange authorizes writes to configured writable DIDs for that TCP
  connection; it does not authorize or change other services.
- Generate deterministic, per-connection 16-bit demonstration seeds. The
  initial seed is `0x1234`; each subsequent request advances it by `0x0101`.
  This is predictable by design and is not a security-strength random source.
- For demonstration only, the expected 16-bit key is the seed XOR `0xA5C3`.
  For example, seed `0x1234` expects key `0xB7F7`.
- Keep the pending seed and unlocked state per connection. A wrong key consumes
  the pending seed. A new connection, diagnostic-session transition, or ECU
  reset starts locked. Once unlocked, another requestSeed returns a zero seed
  and leaves access unlocked.
- No attempt counter, delay timer, persistent credentials, or real ECU key
  derivation is implemented.

## Request and response behavior

All multibyte values use network byte order.

| Request | Positive response | Behavior |
|---|---|---|
| `27 01` | `67 01 <seedHi> <seedLo>` | Creates a pending seed. |
| `27 02 <keyHi> <keyLo>` | `67 02` | Unlocks when the key matches the pending seed's simulator transform. |
| `2E <DID> <data>` in extended session while locked | `7F 2E 33` | Does not modify DID data. |
| `2E <DID> <data>` in extended session after unlock | `6E <DID>` | Writes the configured writable DID. |

Malformed lengths return NRC `0x13`, unsupported subfunctions return `0x12`,
SecurityAccess outside extended session returns `0x7E`, sending a key without
a pending seed returns `0x24`, an incorrect key returns `0x35`, and a locked
DID write returns `0x33`. Existing write validation still returns `0x31` for
unknown or non-writable DIDs and `0x7E` for a write outside extended session.

Session state and authorization are connection-local. Runtime DID values
remain process-shared and are reset to configuration on server restart.

## Verification and live demo

Unit tests cover request sequencing, seed progression, correct and incorrect
keys, malformed requests, write authorization, per-connection state, and
authorization reset. The TCP integration test verifies the seed/key exchange
and guarded DID write within the existing DoIP diagnostic flow.

Run the Debug tests:

```powershell
cmake --build --preset debug
ctest --preset debug --output-on-failure -R "UdsDispatcher|DiagnosticFlowTcp"
```

The [M8 live demo](demo_m8_security_access.md) captures the full flow. The
simulator algorithm is intentionally unsuitable for production.
