# m7_supported_dtcs

This catalog lists the symbolic DTCs currently accepted by the simulator's
`dtcs` configuration. They are anonymized examples from the deployment note,
not detected ECU faults or production diagnostic identifiers.

> [!WARNING]
> The 24-bit values below are synthetic placeholders. Do not treat them as
> authoritative DTCs or transmit them to a production ECU as real diagnostic
> codes. The original DTC-TIs, internal DFCCs, software versions, and software
> clusters are intentionally not included here.

## Supported DTCs

| Symbolic name | Synthetic 24-bit value | Neutral description |
|---|---:|---|
| `DTC_RAM_TEST` | `0xF00001` | RAM self-test fault |
| `DTC_RAM_ECC` | `0xF00002` | RAM ECC fault |
| `DTC_CONTROL_MODULE` | `0xF00003` | Control-module internal fault |
| `DTC_ROM_CHECK` | `0xF00004` | ROM integrity fault |
| `DTC_MEMORY_PROTECTION` | `0xF00005` | Memory-protection violation |
| `DTC_CONTROLLER_SUPERVISION` | `0xF00006` | Controller-supervision fault |
| `DTC_NVM_OPERATION` | `0xF00007` | Non-volatile-memory operation fault |
| `DTC_ADC_CHECK` | `0xF00008` | ADC self-test fault |
| `DTC_POWER_MONITORING` | `0xF00009` | Power-monitoring fault |
| `DTC_SBC_CHECK` | `0xF0000A` | System Basis Chip communication or supervision fault |

These are the only symbolic names accepted in configuration. The server
rejects an unsupported or duplicate name at startup. Configure a DTC by
symbolic name and initial status byte; for example:

```json
{ "symbolic_name": "DTC_RAM_TEST", "initial_status": 9 }
```

The sample `config/ecu_config.json` enables all ten with initial status
`0x09`. The status can be configured from `0x00` through `0xFF`.

## What the simulator does with them

The server does not run RAM, ROM, ADC, power, or other hardware checks to
detect these conditions. It loads the configured records into its shared
process-local DTC store. ReadDTCInformation (`0x19`) reports configured
records that match the requested status mask, using the synthetic value in
the response. ClearDiagnosticInformation (`0x14`) clears the entire runtime
set; restarting the server reloads the configured records.

See the [M7 DTC services guide](m7_dtc_services.md) for request formats,
session requirements, and response behavior.
