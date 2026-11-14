# doip-uds-server

[![CI](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml/badge.svg)](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml)

A C++17 project for building a simulated ECU diagnostic server using DoIP (ISO 13400-2) and UDS (ISO 14229-1), with Raspberry Pi as a target platform.

> **Current status:** M3.5 UDP vehicle-identification discovery and M4.1–M4.2 TCP connection and message framing are implemented. The server handles UDP discovery on port 13400 and accepts multiple concurrent TCP clients, buffering and logging complete DoIP frames with a local 16 MiB per-frame payload limit. Routing activation, diagnostic-message handling, and UDS services are not implemented yet.

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

Requirements: CMake 3.21 or newer, Ninja, and a C++17-capable compiler. CMake selects the compiler available in the environment; the presets do not force GCC, MSVC, or a vcpkg triplet.

GoogleTest is used for unit tests, and `nlohmann/json` parses ECU configuration. CMake uses installed packages if available; otherwise it downloads the dependencies with `FetchContent` during configuration. The fallback therefore requires network access on the first configure. **vcpkg is not used** by the current build.

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
line for each announcement.

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
The server only responds to identification requests; TCP routing activation
and UDS diagnostic services are planned for later milestones.

To see the TCP additions, the [M4.1 demo](docs/demo_m4_1_tcp_connections.md)
shows TCP connections and concurrent clients alongside UDP discovery, while
[M4.2](docs/demo_m4_2_tcp_message_framing.md) captures TCP traffic in Wireshark
and sends messages in separate and coalesced writes, plus an oversized-length
header. TCP payloads are framed and logged, but not yet interpreted or answered.

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
4. With `DOIP_BUILD_TESTS=ON` (the default), it finds or downloads GoogleTest, creates `unit_tests` from `tests/unit/` and `integration_tests` from `tests/integration/`, and registers both with CTest.
5. Ninja executes the generated compile and link steps. CTest runs the registered tests.

The core library currently exposes `doip_uds::version()`, the DoIP generic-header and vehicle-identification payload codecs, ECU identity configuration loading, IPv4 UDP and TCP listeners, bounded TCP message framing, a vehicle-identification datagram handler, and the UDP request/response service. The executable loads the ECU identity, handles UDP discovery, and logs complete DoIP frames received over concurrent TCP connections; it does not yet interpret or respond to TCP payloads. M3.1 and M3.2 standards-specific checks are based on the project's local reference copy of ISO 13400-2:2012(E); this does not imply conformance to later editions or complete protocol conformance. See [`docs/m3_2_ecu_identity_configuration.md`](docs/m3_2_ecu_identity_configuration.md), [`docs/m3_3_udp_transport.md`](docs/m3_3_udp_transport.md), [`docs/m4_1_tcp_transport.md`](docs/m4_1_tcp_transport.md), [`docs/m4_2_tcp_message_framing.md`](docs/m4_2_tcp_message_framing.md), and [`docs/m3_4_vehicle_identification_handler.md`](docs/m3_4_vehicle_identification_handler.md) for implementation details.

## Development direction

Implement protocol logic in the core library, keeping the executable entry point focused on startup and wiring. Put public interfaces in `include/doip_uds/`, implementations in `src/`, unit tests in `tests/unit/`, and network integration tests in `tests/integration/`. The executable loads ECU identity fields from `config/ecu_config.json`; the sample's DID and DTC lists are not consumed yet.

## Roadmap

- [x] M1 Project setup, CI
- [x] M2 DoIP header codec
- [x] M3 UDP vehicle identification
  - [x] **M3.1 — Payload codec:** represent broadcast, VIN-targeted, and EID-targeted identification requests and the identification response; encode/decode their payload bytes. **Done when:** field order, byte order, payload types, and exact sizes are covered by unit tests.
  - [x] **M3.2 — ECU identity configuration:** load the ECU's VIN, logical address, EID, and GID from `config/ecu_config.json`. **Done when:** valid configuration produces a typed identity and invalid/missing fields produce a clear startup error; validation has unit tests.
  - [x] **M3.3 — UDP transport:** create a cross-platform UDP listener on the DoIP port (13400), separate from protocol handling. **Done when:** it can bind, receive datagrams, report bind/receive errors, and shut down cleanly; transport behavior has tests where practical.
  - [x] **M3.4 — Request handling:** connect datagrams, generic-header parsing, M3.1 payload decoding, and the configured identity. **Done when:** valid broadcast and matching VIN/EID requests produce a correctly framed vehicle-identification response, while malformed or non-matching requests do not produce a response.
  - [x] **M3.5 — Integration and usage:** demonstrate startup vehicle announcements and client discovery. **Done when:** an automated integration test verifies UDP request/response; startup sends three broadcast VAMs; and the README and [demo_m3_5_udp_discovery](docs/demo_m3_5_udp_discovery.md) cover startup, packet capture, and the VIR/VAM demo client.
- [ ] M4 TCP routing activation and diagnostic messages
  - [x] **M4.1 — TCP transport:** add a cross-platform TCP listener on DoIP port 13400 and manage accepted connections. **Done when:** connection lifecycle and shutdown are tested, and the supported client/session policy is documented. See the [m4_1_tcp_transport](docs/m4_1_tcp_transport.md) and [demo_m4_1_tcp_connections](docs/demo_m4_1_tcp_connections.md).
  - [x] **M4.2 — TCP message framing:** assemble complete DoIP messages from the TCP byte stream using the generic header. **Done when:** fragmented headers/payloads, multiple messages in one read, and invalid or oversized lengths are handled and tested. See the [m4_2_tcp_message_framing](docs/m4_2_tcp_message_framing.md) and [demo_m4_2_tcp_message_framing](docs/demo_m4_2_tcp_message_framing.md).
  - [ ] **M4.3 — Routing activation:** encode/decode Routing Activation Request (`0x0005`) and Response (`0x0006`), validating requests against the project's ISO reference and tracking activation per connection. **Done when:** supported and rejected activations produce the correct response, with unit and connection-level tests.
  - [ ] **M4.4 — Diagnostic-message transport:** accept Diagnostic Messages (`0x8001`) only after activation, validate logical addresses, send the appropriate ACK (`0x8002`) or NACK (`0x8003`), and hand accepted payloads to a future UDS layer without interpreting services. **Done when:** inactive, invalid-address, and valid-message paths are covered by tests.
  - [ ] **M4.5 — Integration and usage:** verify the TCP connect → routing activation → diagnostic-message flow end to end and document a demo. **Done when:** automated integration tests cover the flow and the demo shows the activation and diagnostic acknowledgment exchange.
- [ ] M5 UDS dispatcher, 0x10 / 0x3E / 0x11
- [ ] M6 0x22 / 0x2E
- [ ] M7 0x19 / 0x14
- [ ] M8 0x27 SecurityAccess
- [ ] M9 Python tester and integration tests
- [ ] M10 Docker, CodeQL, Trivy

M3 completed the UDP vehicle-identification flow, from independently tested
payload and configuration components to end-to-end discovery. M4.1 and M4.2
add TCP transport and message framing; routing activation and
diagnostic-message handling remain upcoming, with UDS services planned for
later milestones.
