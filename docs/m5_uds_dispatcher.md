# m5_uds_dispatcher

**Status:** M5.1 boundary approved; M5.2 services implemented.

## Goal and scope

M5 adds a UDS dispatcher for SessionControl (`0x10`), TesterPresent (`0x3E`),
and ECUReset (`0x11`). This document records the approved boundary and
implemented behavior. The milestone remains one M5 roadmap
item with one M5 guide and one dedicated demo.

## Layering and response flow

- DoIP framing, routing activation, address validation, and Diagnostic Message
  ACK/NACK handling remain in the existing transport layer.
- After a Diagnostic Message passes transport checks, the server sends its
  positive DoIP ACK (`0x8002`) and passes the accepted UDS bytes to a
  transport-independent dispatcher.
- The dispatcher receives the request bytes and that TCP connection's UDS
  state. It returns either no UDS response or response bytes; it does not
  access sockets.
- A UDS response is sent in a DoIP Diagnostic Message (`0x8001`) with the ECU
  as source and the activated tester as target. This is separate from the
  preceding DoIP ACK.
- A suppressed positive UDS response does not suppress the DoIP ACK. Negative
  UDS responses are sent even when the request sets the suppress-positive
  response bit.
- Each TCP connection owns its UDS session state. A new connection starts in
  the default session. An ECU reset returns the simulated session state to
  default without restarting the server process.

## Supported service behavior

| Service | Supported request | Response and state behavior |
|---|---|---|
| SessionControl (`0x10`) | Exactly two bytes; default (`0x01`) and extended (`0x03`) sessions | Positive response `0x50`, echoed subfunction with the suppress bit cleared, P2Server_max `50 ms`, and P2*Server_max `500` 10-ms units. Change the connection's session. |
| TesterPresent (`0x3E`) | Exactly two bytes; subfunction `0x00`, with optional suppress-positive-response bit (`0x80`) | Respond with `0x7E 0x00`, or suppress that positive UDS response. No P3/session timeout is introduced in M5. |
| ECUReset (`0x11`) | Exactly two bytes; hard reset (`0x01`), with optional suppress-positive-response bit (`0x80`) | Simulate and log the reset request, do not restart the process, and return the session state to default. Respond with `0x51 0x01` unless suppressed. |

Supported services are available in both the default and extended sessions.
Programming session and reset types other than hard reset are not supported.

## Negative responses

UDS-level request errors produce `[0x7F, request SID, NRC]`; they do not
replace a DoIP ACK with a DoIP NACK:

| Condition | NRC |
|---|---:|
| Unsupported service ID | `0x11` — service not supported |
| Unsupported service subfunction | `0x12` — subfunction not supported |
| Incorrect request length or format | `0x13` — incorrect message length or invalid format |

DoIP-invalid or misaddressed Diagnostic Messages continue to use the existing
M4.4 transport behavior and do not reach the UDS dispatcher.

## Implementation and verification

The dispatcher is transport-independent and stores the current diagnostic
session in per-connection state. The server sends the DoIP ACK before
dispatching accepted UDS bytes, then sends any UDS response in a separate
Diagnostic Message. A suppressed positive response does not suppress the
transport ACK.

Unit tests cover supported services, negative responses, response suppression,
session transitions, and reset simulation. The loopback TCP integration test
verifies that a DoIP ACK and UDS response are distinct frames on the same
connection. See the [M5 UDS services demo](demo_m5_uds_services.md) for the
live-server flow and capture instructions.

Run the focused tests from the repository root:

```powershell
cmake --build --preset debug
ctest --preset debug --output-on-failure -R "UdsDispatcher|DiagnosticFlowTcp"
```

Out of scope for M5 were DID services (`0x22`/`0x2E`), SecurityAccess
(`0x27`), real hardware or process reset, and inactivity timers. M6 adds the
initial DID services; see the [M6 DID services guide](m6_did_services.md).
