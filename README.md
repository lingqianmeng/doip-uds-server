# doip-uds-server

[![CI](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml/badge.svg)](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml)

A C++17 project for building a simulated ECU diagnostic server using DoIP (ISO 13400-2) and UDS (ISO 14229-1), with Raspberry Pi as a target platform.

> **Current status:** M3.5 UDP discovery, M4.1–M4.5 TCP transport and diagnostic flow, M5.1–M5.2 UDS SessionControl, TesterPresent, and simulated ECUReset, M6.1–M6.2 UDS DataIdentifier services, M7.1–M7.2 UDS DTC services, M8.1–M8.2 simulator SecurityAccess, and M9 Python tester/integration tests are implemented. The server handles UDP discovery on port 13400 and accepts multiple concurrent TCP clients, with a local 16 MiB per-frame payload limit.

## Project layout

```text
apps/doip_server/       Executable entry point
config/                 ECU configuration example
include/doip_uds/       Public headers
src/                    Core library implementation
tests/unit/             GoogleTest unit tests
tests/integration/      Network integration tests
tools/                  Python and PowerShell demo clients
captures/               Local Wireshark captures (pcapng files are ignored)
CMakeLists.txt          Targets, compiler settings, and dependency setup
CMakePresets.json       Debug/release Ninja build presets
docs/                   Feature guides
```

See the [roadmap documentation map](docs/ROADMAP_DOCUMENTATION_MAP.md) to find
guides by milestone and review their PR history.

## Build and test

Requirements: CMake 3.21 or newer, Ninja, Python 3.7 or newer, and a C++17-capable compiler. CMake selects the compiler available in the environment; the presets do not force GCC, MSVC, or a vcpkg triplet.

GoogleTest is used for C++ unit tests, Python's standard library drives the DoIP integration test, and `nlohmann/json` parses ECU configuration. CMake uses installed packages if available; otherwise it downloads the C++ dependencies with `FetchContent` during configuration. The fallback therefore requires network access on the first configure. **vcpkg is not used** by the current build.

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use `release` instead of `debug` for a release build. Presets generate build files under `build/debug` and `build/release`.

Run the server from the repository root so it can find the default ECU configuration. It listens on all local IPv4 interfaces at UDP port 13400 and accepts TCP connections on TCP port 13400; press Ctrl+C to stop it.

```sh
# Linux
./build/debug/doip_server

# Windows PowerShell
.\build\debug\doip_server.exe

# Optional ECU configuration path
./build/debug/doip_server config/ecu_config.json
```

The server broadcasts three startup VAMs to `255.255.255.255:13400`, with a
random initial delay of up to 500 ms and 500 ms between announcements. To see
the broadcast on the network, start Wireshark on the active network adapter
before starting the server and use the display filter `udp.port == 13400`.
Each VAM is a DoIP payload type `0x0004`; the VIN is in the first 17 payload
bytes (UDP payload offsets 8–24). The server also prints a `[VAM startup]` log
line for each announcement. Sending an announcement is best effort. If the
host rejects the limited broadcast, the server logs
`[VAM startup] Warning: announcement 1/3 failed: ...` and still waits before
the remaining announcements. The process keeps serving TCP diagnostics and
UDP identification requests. A failed identification response is logged as
`[VAM response] Warning: ...` and does not stop the server unless the UDP
socket itself is unusable. Failure to bind a port, or to load the ECU
configuration, is still fatal.

```sh
# Discover on the local network (broadcast VIR, unicast VAM response)
python tools/doip_vir_vam_demo.py

# Or target loopback when both programs run on the same computer
python tools/doip_vir_vam_demo.py --destination 127.0.0.1
```

On Windows PowerShell, the same demo is available without Python:

```powershell
.\tools\doip_vir_vam_demo.ps1
.\tools\doip_vir_vam_demo.ps1 -Destination 127.0.0.1
```

For the complete Windows loopback-capture walkthrough using Scoop Wireshark,
see [demo_m3_5_udp_discovery](docs/demo_m3_5_udp_discovery.md).

The demo prints the responding ECU's VIN, logical address, EID, and GID. The
server logs the reply as `[VAM response]`, distinct from startup announcements.
On the Wireshark capture, the VIR has payload type `0x0001`; its VAM response
has type `0x0004` and contains the same identity fields.
The server responds to UDP vehicle-identification requests and TCP routing
activation requests. After acknowledging a valid diagnostic message at the
DoIP transport layer, it dispatches UDS SessionControl (`0x10`), TesterPresent
(`0x3E`), simulated ECUReset (`0x11`), ReadDataByIdentifier (`0x22`), and
WriteDataByIdentifier (`0x2E`) requests.
ReadDTCInformation (`0x19`) supports status-masked count and record reports,
and ClearDiagnosticInformation (`0x14`) clears all configured synthetic DTCs
in extended session. SecurityAccess (`0x27`) demonstrates the poster-listed
Bootloader and AirbagDeployment seed/key pairs and gates writes to the sample
writable DID.

To see the TCP additions, the [M4.1 demo](docs/demo_m4_1_tcp_connections.md)
shows TCP connections and concurrent clients alongside UDP discovery, while
[M4.2](docs/demo_m4_2_tcp_message_framing.md) captures TCP traffic in Wireshark
and demonstrates split/coalesced writes and an oversized-length header.
The dedicated [M4.3 demo](docs/demo_m4_3_routing_activation.md) checks routing
activation success and rejection responses over separate TCP connections. The
[M4.4 demo](docs/demo_m4_4_diagnostic_message_transport.md) checks diagnostic
message ACK/NACK behavior after activation.
The [M4.5 demo](docs/demo_m4_5_tcp_diagnostic_flow.md) captures startup VAMs
and a VIR/VAM discovery exchange alongside the connect → activation →
diagnostic-message ACK and UDS VIN-read exchange on one TCP connection.
The dedicated [M5 demo](docs/demo_m5_uds_services.md) shows the separate DoIP
ACK and UDS responses for the three initial UDS services.
The [M6 DID services demo](docs/demo_m6_did_services.md) reads the configured
VIN and simulated DID, then demonstrates extended-session DID writes.
The [M7 DTC services demo](docs/demo_m7_dtc_services.md) reads synthetic DTC
counts and records, then clears the runtime DTC set.
The [M8 SecurityAccess demo](docs/demo_m8_security_access.md) shows the
simulator seed/key sequence guarding a DID write.
The [M9 Python tester demo](docs/demo_m9_python_tester.md) exercises the local
simulator through a standard-library DoIP client.
The [Docker run and test guide](docs/m10_3_docker_run_and_test.md) builds the
image, publishes TCP and UDP port 13400, and runs that tester against the
container.

Without presets, a debug build can be configured explicitly:

```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

## How the build is assembled

1. CMake reads `CMakeLists.txt` and configures a C++17 build with strict warnings.
2. It creates `doip_uds_core` from the implementation in `src/`, exposing headers under `include/`.
3. It creates `doip_server` from `apps/doip_server/main.cpp`, linked to the core library.
4. With `DOIP_BUILD_TESTS=ON` (the default), it finds or downloads GoogleTest, creates the C++ unit/integration test targets, and registers them plus the Python DoIP tester integration test with CTest.
5. Ninja executes the generated compile and link steps. CTest runs the registered tests.

The core library exposes DoIP and vehicle-identification codecs, UDP/TCP
transport, routing activation, diagnostic-message policy, and a
transport-independent UDS dispatcher with per-connection session state. The
executable handles UDP discovery and TCP routing activation, sends DoIP
Diagnostic Message ACK/NACK frames, then dispatches the M5–M8 UDS services
and sends any UDS response in a reverse-addressed Diagnostic Message. ECUReset
is simulated and does not restart the process. M3.1 and M3.2
standards-specific checks are based on the project's local reference copy of
ISO 13400-2:2012(E).
M4.3 and M4.4 implement version-0x02 message formats and selected address
policies; they do not claim complete conformance to this or later editions.
See [`docs/m3_2_ecu_identity_configuration.md`](docs/m3_2_ecu_identity_configuration.md),
[`docs/m3_3_udp_transport.md`](docs/m3_3_udp_transport.md),
[`docs/m4_1_tcp_transport.md`](docs/m4_1_tcp_transport.md),
[`docs/m4_2_tcp_message_framing.md`](docs/m4_2_tcp_message_framing.md),
[`docs/m4_3_routing_activation.md`](docs/m4_3_routing_activation.md),
[`docs/m4_4_diagnostic_message_transport.md`](docs/m4_4_diagnostic_message_transport.md),
[`docs/m4_5_tcp_diagnostic_flow.md`](docs/m4_5_tcp_diagnostic_flow.md),
[`docs/m5_uds_dispatcher.md`](docs/m5_uds_dispatcher.md),
[`docs/m8_security_access.md`](docs/m8_security_access.md), and
[`docs/m3_4_vehicle_identification_handler.md`](docs/m3_4_vehicle_identification_handler.md)
for implementation details.

## Development direction

Implement protocol logic in the core library, keeping the executable entry point focused on startup and wiring. Put public interfaces in `include/doip_uds/`, implementations in `src/`, unit tests in `tests/unit/`, and network integration tests in `tests/integration/`. The executable loads ECU identity fields, DIDs, and symbolic synthetic DTCs from `config/ecu_config.json`.

## Roadmap

- [x] M1 Project setup, CI
- [x] M2 DoIP header codec
- [x] M3 UDP vehicle identification
  - [x] **M3.1 — Payload codec:** represent broadcast, VIN-targeted, and EID-targeted identification requests and the identification response; encode/decode their payload bytes. **Done when:** field order, byte order, payload types, and exact sizes are covered by unit tests.
  - [x] **M3.2 — ECU identity configuration:** load the ECU's VIN, logical address, EID, and GID from `config/ecu_config.json`. **Done when:** valid configuration produces a typed identity and invalid/missing fields produce a clear startup error; validation has unit tests.
  - [x] **M3.3 — UDP transport:** create a cross-platform UDP listener on the DoIP port (13400), separate from protocol handling. **Done when:** it can bind, receive datagrams, report bind/receive errors, and shut down cleanly; transport behavior has tests where practical.
  - [x] **M3.4 — Request handling:** connect datagrams, generic-header parsing, M3.1 payload decoding, and the configured identity. **Done when:** valid broadcast and matching VIN/EID requests produce a correctly framed vehicle-identification response, while malformed or non-matching requests do not produce a response.
  - [x] **M3.5 — Integration and usage:** demonstrate startup vehicle announcements and client discovery. **Done when:** an automated integration test verifies UDP request/response; startup sends three broadcast VAMs; and the README and [demo_m3_5_udp_discovery](docs/demo_m3_5_udp_discovery.md) cover startup, packet capture, and the VIR/VAM demo client.
- [x] M4 TCP routing activation and diagnostic messages
  - [x] **M4.1 — TCP transport:** add a cross-platform TCP listener on DoIP port 13400 and manage accepted connections. **Done when:** connection lifecycle and shutdown are tested, and the supported client/session policy is documented. See the [m4_1_tcp_transport](docs/m4_1_tcp_transport.md) and [demo_m4_1_tcp_connections](docs/demo_m4_1_tcp_connections.md).
  - [x] **M4.2 — TCP message framing:** assemble complete DoIP messages from the TCP byte stream using the generic header. **Done when:** fragmented headers/payloads, multiple messages in one read, and invalid or oversized lengths are handled and tested. See the [m4_2_tcp_message_framing](docs/m4_2_tcp_message_framing.md) and [demo_m4_2_tcp_message_framing](docs/demo_m4_2_tcp_message_framing.md).
  - [x] **M4.3 — Routing activation:** encode/decode Routing Activation Request (`0x0005`) and Response (`0x0006`), apply the selected Default-activation and test-equipment-address policies, and track activation per connection. The server rejects unsupported activation types and duplicate active addresses. See the [m4_3_routing_activation](docs/m4_3_routing_activation.md).
  - [x] **M4.4 — Diagnostic-message transport:** accept Diagnostic Messages (`0x8001`) only after activation, require the source to match the active tester and the target to match the configured ECU, and send ACK (`0x8002`) or NACK (`0x8003`) responses. M4.4 preserves accepted UDS bytes for the dispatcher added in M5. See the [m4_4_diagnostic_message_transport](docs/m4_4_diagnostic_message_transport.md) and [demo_m4_4_diagnostic_message_transport](docs/demo_m4_4_diagnostic_message_transport.md).
  - [x] **M4.5 — Integration and usage:** verify TCP connect → routing activation → diagnostic-message flow on one connection and document the live exchange. The integration test checks the routing response, diagnostic ACK, and preserved UDS bytes. See the [m4_5_tcp_diagnostic_flow](docs/m4_5_tcp_diagnostic_flow.md) and [demo_m4_5_tcp_diagnostic_flow](docs/demo_m4_5_tcp_diagnostic_flow.md).
- [x] M5 UDS dispatcher, 0x10 / 0x3E / 0x11
  - [x] **M5.1 — Define the UDS boundary:** agreed on the dispatcher interface, per-connection UDS state, the DoIP ACK versus UDS response flow, supported subfunctions, timing values, and negative-response behavior. See the [M5 UDS boundary](docs/m5_uds_dispatcher.md).
  - [x] **M5.2 — Build the UDS services:** implemented SessionControl (`0x10`), TesterPresent (`0x3E`), and simulated ECUReset (`0x11`); integrated the dispatcher with TCP diagnostics; added unit and integration tests; and provided one M5 guide and one dedicated M5 demo. See the [M5 UDS dispatcher guide](docs/m5_uds_dispatcher.md) and [M5 UDS services demo](docs/demo_m5_uds_services.md).
- [x] M6 UDS DataIdentifier services, 0x22 / 0x2E
  - [x] **M6.1 — Define the DID boundary:** agreed on the initial DID catalog, read/write access, multi-DID reads, in-memory write behavior, session rules, and negative responses. See the [M6 DID boundary](docs/m6_did_services.md).
  - [x] **M6.2 — Build the DID services:** implemented ReadDataByIdentifier (`0x22`) and WriteDataByIdentifier (`0x2E`) against the agreed DID model; added unit/integration tests and a dedicated M6 demo. See the [M6 DID services guide](docs/m6_did_services.md) and [M6 DID services demo](docs/demo_m6_did_services.md).
- [x] M7 UDS DTC services, 0x19 / 0x14
  - [x] **M7.1 — Define the DTC boundary:** agreed on symbolic DTC configuration, supported ReadDTCInformation (`0x19`) subfunctions and status-mask filters, ClearDiagnosticInformation (`0x14`) scope, runtime clear behavior, session rules, and negative responses. See the [M7 DTC boundary](docs/m7_dtc_services.md).
  - [x] **M7.2 — Build the DTC services:** implemented the agreed DTC model and `0x19`/`0x14` behavior; added configuration, unit, and integration tests plus a dedicated M7 demo. See the [M7 DTC services guide](docs/m7_dtc_services.md) and [M7 DTC services demo](docs/demo_m7_dtc_services.md).
- [x] M8 UDS SecurityAccess, 0x27
  - [x] **M8.1 — Define the SecurityAccess boundary:** define the simulator security level, seed/key exchange, per-connection authorization, protected DID operation, state reset behavior, and negative responses. See the [M8 SecurityAccess guide](docs/m8_security_access.md).
  - [x] **M8.2 — Build the SecurityAccess service:** implement the simulator-only `0x27` exchange, guard writable DID access, and add dispatcher/TCP tests and a live demo.
- [x] M9 Python tester and integration tests
  - [x] **M9.1 — Define the tester boundary:** keep the standard-library client local to the simulator, validate DoIP framing and ACK/UDS response sequencing, and avoid arbitrary UDS or key-guessing modes. See the [M9 Python tester guide](docs/m9_python_tester.md).
  - [x] **M9.2 — Build the tester and integration test:** add the Python smoke-test CLI, configurable loopback/ephemeral server ports, and a CTest integration test. See the [M9 Python tester demo](docs/demo_m9_python_tester.md).
- [ ] M10 native and Docker deployment, CodeQL, Trivy
  - [x] **M10.1 — Define the deployment boundary:** describe native `.deb`/`systemd` and Docker deployment, networking, security scanning, and verification. See the [M10.1 deployment boundary](docs/m10_1_deployment_boundary.md).
  - [ ] **M10.2 — Native Debian deployment:** add graceful shutdown, package the server as a `.deb` with a `systemd` service, and verify package/service lifecycle on Linux.
  - [ ] **M10.3 — Docker deployment:** build a non-root runtime image, test TCP/UDP behavior and SIGTERM shutdown, and document host/bridge networking. See the [M10.3 Docker demo](docs/demo_m10_3_docker.md) and the [Docker run and test guide](docs/m10_3_docker_run_and_test.md).
  - [ ] **M10.4 — Security scanning and documentation:** add CodeQL and Trivy workflows, deployment guides/demos, and final README/roadmap links.

M3 completed the UDP vehicle-identification flow, from independently tested
payload and configuration components to end-to-end discovery. M4.1–M4.5 are
complete, adding TCP transport, message framing, routing activation,
diagnostic-message acknowledgement, and the integrated TCP diagnostic flow.
M5.1 defined and reviewed the UDS boundary; M5.2 implemented the three initial
services under one milestone and one dedicated demo. M6.1 defined the DID
boundary; M6.2 implemented `0x22` and `0x2E` with a dedicated guide and demo.
M7.1 defined the symbolic DTC boundary; M7.2 implemented status-filtered
`0x19` reads and `0x14` clear-all with configuration, tests, and a dedicated
demo. M8.1 defined the simulator-only SecurityAccess boundary; M8.2 implements
`0x27` and uses its per-connection authorization to guard DID writes. M9 adds a
loopback-only Python tester and launches it against an isolated server process
in CTest. M10.1 defines the deployment boundary; M10.2–M10.4 remain upcoming.
