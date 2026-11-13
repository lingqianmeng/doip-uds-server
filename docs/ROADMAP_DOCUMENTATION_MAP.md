# Roadmap and documentation map

This index connects implemented roadmap steps to their detailed guides and
records which merged pull requests introduced or later updated those guides.
It is a map of the current documentation, not a promise that every PR has a
separate document.

## Implemented milestones

| Roadmap step | Guide | Documentation history |
|---|---|---|
| M1 — Project setup and CI | `README.md` | Initial project setup and build instructions were introduced in PR #1. No separate M1 guide. |
| M2 — DoIP generic-header codec | [DoIP header codec](DOIP_HEADER_CODEC.md) | Guide added in PR #4 after implementation PR #3. |
| M3.1 — Vehicle-identification payload codec | [Vehicle-identification codec](VEHICLE_IDENTIFICATION_CODEC.md) | Guide added with implementation PR #6; standards alignment and guide updates followed in PR #7. |
| M3.2 — ECU identity configuration | [ECU identity configuration](ECU_IDENTITY_CONFIGURATION.md) | Guide added with implementation PR #8. |
| M3.3 — UDP transport | [UDP transport](UDP_TRANSPORT.md) | Guide added with implementation PR #9 and expanded in documentation PR #10. |
| M3.4 — Vehicle-identification request handler | [Vehicle-identification handler](VEHICLE_IDENTIFICATION_HANDLER.md) | Guide added with implementation PR #11 and updated in PR #12. |
| M3.5 — UDP discovery integration and usage | [UDP transport](UDP_TRANSPORT.md), [vehicle-identification handler](VEHICLE_IDENTIFICATION_HANDLER.md), [DoIP UDP discovery demo](DOIP_DISCOVERY_DEMO.md), and [README run instructions](../README.md#build-and-test) | PR #12 updated the original guides and README; the dedicated walkthrough records the Scoop Wireshark loopback capture and VIR/VAM steps. |
| M4.1 — TCP transport | [TCP transport](TCP_TRANSPORT.md) and [M4.1 demo variant](DOIP_DISCOVERY_DEMO_M4_1.md) | Adds the TCP listener, concurrent connection lifecycle, transport tests, and a capture/demo variant that preserves the original UDP guide. |

The M3 breakdown itself was recorded in the README in PR #5. The detailed
guides explain the implementation and tests for individual features; the
README remains the top-level overview and setup guide.

## Upcoming roadmap steps

M4 is in progress: M4.1 is complete and has a guide, while M4.2–M4.5 remain
unchecked. M5–M10 are upcoming. When a step is implemented, add or update its
guide and update this table in the same PR. A dedicated guide is useful when a
feature needs more explanation than belongs in the README; it does not need
to be created for every small PR.

## How to verify the history

From the repository root, inspect documentation changes with:

```sh
git log --oneline --name-status -- docs README.md
```

`A` means a guide was added; `M` means an existing document was updated. The
PR references above come from the corresponding merge commit subjects in the
repository history.
