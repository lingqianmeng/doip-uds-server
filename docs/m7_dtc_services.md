# m7_dtc_services

**Status:** M7.1 boundary agreed; M7.2 implementation complete.

## Goal

Define a limited, testable DTC model for ReadDTCInformation (`0x19`) and
ClearDiagnosticInformation (`0x14`). M7 remains one roadmap milestone, split
into M7.1 (boundary) and M7.2 (implementation).

## Existing project context

The ECU configuration already contains a `dtcs` array that M3.2 left for a
later diagnostic-service milestone. M5/M6 established the transport-independent
dispatcher and shared process-local service data, while diagnostic-session
state remains per TCP connection.

## Proposed boundary

The M7 boundary and implementation are:

- Keep DoIP framing, routing, ACK/NACK, and ACK-before-UDS-response ordering
  unchanged. DTC selection and service validation remain in the UDS layer.
- Configure DTCs under the existing `dtcs` array using symbolic names and an
  initial status byte. The ten available names map internally to synthetic,
  simulator-only 24-bit values:

  | Symbolic name | Synthetic value |
  |---|---:|
  | `DTC_RAM_TEST` | `0xF00001` |
  | `DTC_RAM_ECC` | `0xF00002` |
  | `DTC_CONTROL_MODULE` | `0xF00003` |
  | `DTC_ROM_CHECK` | `0xF00004` |
  | `DTC_MEMORY_PROTECTION` | `0xF00005` |
  | `DTC_CONTROLLER_SUPERVISION` | `0xF00006` |
  | `DTC_NVM_OPERATION` | `0xF00007` |
  | `DTC_ADC_CHECK` | `0xF00008` |
  | `DTC_POWER_MONITORING` | `0xF00009` |
  | `DTC_SBC_CHECK` | `0xF0000A` |

  These values are invented placeholders for this simulator, not production
  DTCs or authoritative diagnostic identifiers. Configuration, logs, and tests
  use symbolic names; the sample gives all ten an initial status of `0x09`.
  Seed the process-wide runtime store from configuration; clear operations
  modify only runtime state, which is restored from configuration on restart.
- Support `0x19` subfunctions `0x01` (report number of DTCs matching a status
  mask) and `0x02` (report matching DTC/status records). Use DTC format
  identifier `0x01`; match a DTC when its current status intersects the
  requested status mask. Return zero matches as a valid response.
- Support `0x14` only for DTC group `0xFFFFFF` (all groups). A successful clear
  removes the configured DTCs from the runtime active set; a server restart
  re-seeds the initial configured set.
- Allow reads in default and extended sessions. Require extended session for
  clear. SecurityAccess (`0x27`) remains outside M7.
- Use NRC `0x13` for malformed request lengths, `0x12` for unsupported `0x19`
  subfunctions, `0x31` for an unsupported DTC group, and `0x7E` when clearing
  is requested outside the extended session.
- Include all ten symbolic sample DTCs in the example configuration and
  demonstrate count/list reads followed by a clear and empty result.

## Configuration and examples

Each config entry uses `symbolic_name` and a decimal `initial_status` byte, for
example:

```json
{ "symbolic_name": "DTC_RAM_TEST", "initial_status": 9 }
```

The service returns the synthetic 24-bit value on the wire. These limited
services follow the layouts and response policy defined for this project and
do not claim complete ISO 14229 conformance. See the
[M7 live demo](demo_m7_dtc_services.md) for request/response exchanges and a
loopback capture walkthrough.
