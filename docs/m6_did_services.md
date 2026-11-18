# m6_did_services

**Status:** M6.1 boundary defined; M6.2 implemented.

## Goal

Define a small, testable ECU Data Identifier (DID) model for ReadDataByIdentifier
(`0x22`) and WriteDataByIdentifier (`0x2E`). M6 remains one roadmap milestone,
split into M6.1 (boundary) and M6.2 (implementation).

## Existing project context

The ECU configuration already contains a `dids` field, which M3.2 explicitly
left unused for a later diagnostic-service milestone. The configured VIN is
also available as a candidate read-only DID (`0xF190`). M5 established that the
UDS dispatcher is transport-independent and that the server sends the DoIP
Diagnostic Message ACK before any separate UDS response.

## DID model and access

- Keep DoIP framing, routing, ACK/NACK, and response ordering unchanged. DID
  lookup and service validation remain in the transport-independent UDS layer.
- `0xF190` is a read-only DID sourced from the configured 17-byte VIN.
- `0xF1A0` is the M6 simulation DID: a four-byte opaque value, initially
  `00 00 00 00`, readable and writable. It is project-defined demo data, not a
  claim that the DID has a standardized vehicle meaning.
- The existing configuration `dids` array seeds writable DID values. M6 writes
  update runtime memory only; values return to their configured initial value
  when the server restarts. They do not rewrite configuration files.
- A `0x22` request may contain one or more DIDs. The positive response contains
  each DID and its data in request order. If any requested DID is unknown or
  unreadable, return one negative response rather than a partial positive
  response.
- A `0x2E` request writes exactly one DID. Its data length must match the
  configured value length. A successful write responds with `0x6E` and the
  written DID.
- Reads are available in default and extended sessions. Writes require the
  extended session and, since M8, an unlocked simulator security level. See the
  [M8 SecurityAccess guide](m8_security_access.md).
- Malformed request lengths use NRC `0x13`; unsupported DIDs or access
  directions use NRC `0x31`; writes in the default session use NRC `0x7E`.
- The existing DoIP ACK-before-UDS-response flow and positive-response
  suppression behavior remain unchanged.

`0xF190` is built from the configured VIN and is not configurable. The optional
`dids` configuration array defines simulated DID values. For example:

```json
{
  "identifier": "0xF1A0",
  "initial_value": [0, 0, 0, 0],
  "readable": true,
  "writable": true
}
```

The loader validates hexadecimal identifiers, non-empty byte arrays, access
flags, and duplicate IDs. It rejects entries that override `0xF190` or have
neither read nor write access. DID values are held in one process-wide store
shared by TCP clients; session state remains per connection.

## Service behavior

| Service | Request | Positive response | Negative responses |
|---|---|---|---|
| ReadDataByIdentifier (`0x22`) | `22` followed by one or more two-byte DIDs | `62`, then each DID and its data in request order | `0x13` for empty/incomplete DID lists; `0x31` for an unknown or unreadable DID. A request with any unavailable DID gets no partial positive response. |
| WriteDataByIdentifier (`0x2E`) | `2E`, one two-byte DID, then exactly its configured number of data bytes | `6E` followed by the written DID | `0x13` for missing or incorrect-length data; `0x31` for an unknown or non-writable DID; `0x7E` outside extended session; `0x33` until SecurityAccess is unlocked. |

Reads are available in default and extended sessions. Writes require extended
session and an unlocked simulator security level. A successful write updates
the shared runtime value; it is not persistent and returns to the configured
initial value when the server restarts.

DoIP behavior is unchanged: the server sends the Diagnostic Message ACK first,
then any UDS positive or negative response in a separate reverse-addressed
Diagnostic Message.

## Tests and demo

Unit tests cover VIN reads, ordered multi-DID reads, unknown and unreadable
DIDs, write permissions, session restrictions, value-length validation, and
shared values across connection states. Configuration tests cover DID parsing
and invalid entries. The TCP integration test verifies ACK-before-UDS response
ordering for reads and writes.

Run the focused tests from the repository root:

```powershell
cmake --build --preset debug
ctest --preset debug --output-on-failure -R "EcuIdentity|UdsDispatcher|DiagnosticFlowTcp"
```

See the [M6 DID services demo](demo_m6_did_services.md) for a live-server
walkthrough of reads, rejected writes before extended session and SecurityAccess,
the M8 seed/key exchange, and verification of the updated value.
