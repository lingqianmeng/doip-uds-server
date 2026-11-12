# doip-uds-server

[![CI](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml/badge.svg)](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml)

A C++17 project for building a simulated ECU diagnostic server using DoIP (ISO 13400-2) and UDS (ISO 14229-1), with Raspberry Pi as a target platform.

> **Current status:** M3.5 UDP vehicle-identification discovery is implemented. The server loads ECU identity, broadcasts three startup vehicle-identification announcements, and replies to broadcast or matching VIN/EID requests. TCP diagnostic connections and UDS services are not implemented yet.

## Project layout

```text
apps/doip_server/       Executable entry point
config/                 ECU configuration example
include/doip_uds/       Public headers
src/                    Core library implementation
tests/unit/             GoogleTest unit tests
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

Run the UDP discovery server from the repository root so it can find the default ECU configuration. It listens on all local IPv4 interfaces at UDP port 13400; press Ctrl+C to stop it.

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
see the [DoIP UDP discovery demo guide](docs/DOIP_DISCOVERY_DEMO.md).

The demo prints the responding ECU's VIN, logical address, EID, and GID. The
server logs the reply as `[VAM response]`, distinct from startup announcements.
On the Wireshark capture, the VIR has payload type `0x0001`; its VAM response
has type `0x0004` and contains the same identity fields.
The server only responds to identification requests; TCP routing activation
and UDS diagnostic services are planned for later milestones.

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

The core library currently exposes `doip_uds::version()`, the DoIP generic-header and vehicle-identification payload codecs, ECU identity configuration loading, the IPv4 UDP listener, a vehicle-identification datagram handler, and the UDP request/response service. The executable loads the ECU identity, listens for identification requests, and sends framed responses to matching clients. M3.1 and M3.2 standards-specific checks are based on the project's local reference copy of ISO 13400-2:2012(E); this does not imply conformance to later editions or complete protocol conformance. See [`docs/ECU_IDENTITY_CONFIGURATION.md`](docs/ECU_IDENTITY_CONFIGURATION.md), [`docs/UDP_TRANSPORT.md`](docs/UDP_TRANSPORT.md), and [`docs/VEHICLE_IDENTIFICATION_HANDLER.md`](docs/VEHICLE_IDENTIFICATION_HANDLER.md) for implementation details.

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
  - [x] **M3.5 — Integration and usage:** demonstrate startup vehicle announcements and client discovery. **Done when:** an automated integration test verifies UDP request/response; startup sends three broadcast VAMs; and the README and [discovery demo guide](docs/DOIP_DISCOVERY_DEMO.md) cover startup, packet capture, and the VIR/VAM demo client.
- [ ] M4 TCP routing activation and diagnostic messages
- [ ] M5 UDS dispatcher, 0x10 / 0x3E / 0x11
- [ ] M6 0x22 / 0x2E
- [ ] M7 0x19 / 0x14
- [ ] M8 0x27 SecurityAccess
- [ ] M9 Python tester and integration tests
- [ ] M10 Docker, CodeQL, Trivy

M3's goal is limited to discovering the simulated ECU over UDP and returning
its configured identity. It does not include TCP diagnostic connections or
UDS services; those are later milestones. Complete each M3 sub-milestone in
its own feature branch and PR. The sequence keeps payload/configuration logic
independently testable before socket I/O is added, then verifies the assembled
behavior end to end.
