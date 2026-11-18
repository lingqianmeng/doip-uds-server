# Roadmap and documentation map

This index connects implemented roadmap steps to their detailed guides and
records which merged pull requests introduced or later updated those guides.
It is a map of the current documentation, not a promise that every PR has a
separate document.

Demo documents follow the pattern `demo_m<major>_<minor>_<topic>.md`; their
headings use the same lowercase underscore-separated name without `.md`.
Milestone guides follow `m<major>_<minor>_<topic>.md` with matching headings.
For milestones without a minor number, omit that segment (for example,
`m2_doip_header_codec.md`).

## Implemented milestones

| Roadmap step | Guide | Documentation history |
|---|---|---|
| M1 — Project setup and CI | `README.md` | Initial project setup and build instructions were introduced in PR #1. No separate M1 guide. |
| M2 — DoIP generic-header codec | [m2_doip_header_codec](m2_doip_header_codec.md) | Guide added in PR #4 after implementation PR #3. |
| M3.1 — Vehicle-identification payload codec | [m3_1_vehicle_identification_codec](m3_1_vehicle_identification_codec.md) | Guide added with implementation PR #6; standards alignment and guide updates followed in PR #7. |
| M3.2 — ECU identity configuration | [m3_2_ecu_identity_configuration](m3_2_ecu_identity_configuration.md) | Guide added with implementation PR #8. |
| M3.3 — UDP transport | [m3_3_udp_transport](m3_3_udp_transport.md) | Guide added with implementation PR #9 and expanded in documentation PR #10. |
| M3.4 — Vehicle-identification request handler | [m3_4_vehicle_identification_handler](m3_4_vehicle_identification_handler.md) | Guide added with implementation PR #11 and updated in PR #12. |
| M3.5 — UDP discovery integration and usage | [m3_3_udp_transport](m3_3_udp_transport.md), [m3_4_vehicle_identification_handler](m3_4_vehicle_identification_handler.md), [demo_m3_5_udp_discovery](demo_m3_5_udp_discovery.md), and [README run instructions](../README.md#build-and-test) | PR #12 updated the original guides and README; the dedicated walkthrough records the Scoop Wireshark loopback capture and VIR/VAM steps. |
| M4.1 — TCP transport | [m4_1_tcp_transport](m4_1_tcp_transport.md) and [demo_m4_1_tcp_connections](demo_m4_1_tcp_connections.md) | Adds the TCP listener, concurrent connection lifecycle, transport tests, and a capture/demo variant that preserves the original UDP guide. |
| M4.2 — TCP message framing | [m4_2_tcp_message_framing](m4_2_tcp_message_framing.md) and [demo_m4_2_tcp_message_framing](demo_m4_2_tcp_message_framing.md) | Adds bounded DoIP stream framing with tests, plus its own PowerShell walkthrough for fragmented, coalesced, and oversized messages. |
| M4.3 — Routing activation | [m4_3_routing_activation](m4_3_routing_activation.md) and [demo_m4_3_routing_activation](demo_m4_3_routing_activation.md) | Adds version-0x02 routing-activation codecs, per-connection state, response policy, TCP connection-level tests, and a dedicated activation demo. |
| M4.4 — Diagnostic-message transport | [m4_4_diagnostic_message_transport](m4_4_diagnostic_message_transport.md) and [demo_m4_4_diagnostic_message_transport](demo_m4_4_diagnostic_message_transport.md) | Adds activated-session and logical-address checks, diagnostic ACK/NACK responses, preserved UDS payload handoff, tests, and a dedicated transport demo. |
| M4.5 — TCP diagnostic flow integration and usage | [m4_5_tcp_diagnostic_flow](m4_5_tcp_diagnostic_flow.md) and [demo_m4_5_tcp_diagnostic_flow](demo_m4_5_tcp_diagnostic_flow.md) | Verifies activation and diagnostic ACK on one TCP connection with an integration test and dedicated live demo. |
| M5 — UDS dispatcher and basic services | [m5_uds_dispatcher](m5_uds_dispatcher.md) and [demo_m5_uds_services](demo_m5_uds_services.md) | Adds per-connection diagnostic-session state, SessionControl, TesterPresent, simulated ECUReset, tests, and one dedicated services demo. |
| M6 — UDS DataIdentifier services | [m6_did_services](m6_did_services.md) and [demo_m6_did_services](demo_m6_did_services.md) | Defines the DID boundary, reads configured VIN and writable DIDs, supports extended-session writes with process-local state, and includes unit/integration tests and a live demo. |
| M7 — UDS DTC services | [m7_dtc_services](m7_dtc_services.md), [m7_supported_dtcs](m7_supported_dtcs.md), and [demo_m7_dtc_services](demo_m7_dtc_services.md) | Documents the supported symbolic simulator-only DTCs, implements status-filtered `0x19` count/list and extended-session `0x14` clear-all, and includes configuration, unit/integration tests, and a live demo. |

The M3 breakdown itself was recorded in the README in PR #5. The detailed
guides explain the implementation and tests for individual features; the
README remains the top-level overview and setup guide.

## Upcoming roadmap steps

M4.1–M7 are complete and have guides. M8–M10 are upcoming. M5, M6, and M7 were
each delivered as one milestone in two sub-milestones: a boundary-definition
phase followed by implementation with one guide and one dedicated demo. When a
step is implemented, add or update its guide and update this table in the same
PR.
A dedicated guide is useful when a feature needs more explanation than belongs
in the README; it does not need to be created for every small PR.

## How to verify the history

From the repository root, inspect documentation changes with:

```sh
git log --oneline --name-status -- docs README.md
```

`A` means a guide was added; `M` means an existing document was updated. The
PR references above come from the corresponding merge commit subjects in the
repository history.
