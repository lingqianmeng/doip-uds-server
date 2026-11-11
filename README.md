# doip-uds-server

[![CI](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml/badge.svg)](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml)

A C++17 project for building a simulated ECU diagnostic server using DoIP (ISO 13400-2) and UDS (ISO 14229-1), with Raspberry Pi as a target platform.

> **Current status:** M3.5 UDP vehicle-identification discovery is implemented. The server loads ECU identity, listens on UDP port 13400, and replies to broadcast or matching VIN/EID requests. TCP diagnostic connections and UDS services are not implemented yet.

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

Check discovery from another terminal on the same computer with Python:

```sh
python -c "import socket,struct; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(3); s.sendto(bytes.fromhex('ff 00 00 01 00 00 00 00'),('127.0.0.1',13400)); d,_=s.recvfrom(1024); v,iv,t,n=struct.unpack('>BBHI',d[:8]); assert (v,iv,t,n)==(2,253,4,33) and len(d)==41; print('ECU VIN:',d[8:25].decode('ascii')); print('Logical address: 0x%04X'%int.from_bytes(d[25:27],'big'))"
```

The request is a broadcast vehicle-identification request sent to loopback. A
successful discovery prints the ECU's configured VIN and logical address.
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
- [ ] M3 UDP vehicle identification
  - [x] **M3.1 — Payload codec:** represent broadcast, VIN-targeted, and EID-targeted identification requests and the identification response; encode/decode their payload bytes. **Done when:** field order, byte order, payload types, and exact sizes are covered by unit tests.
  - [x] **M3.2 — ECU identity configuration:** load the ECU's VIN, logical address, EID, and GID from `config/ecu_config.json`. **Done when:** valid configuration produces a typed identity and invalid/missing fields produce a clear startup error; validation has unit tests.
  - [x] **M3.3 — UDP transport:** create a cross-platform UDP listener on the DoIP port (13400), separate from protocol handling. **Done when:** it can bind, receive datagrams, report bind/receive errors, and shut down cleanly; transport behavior has tests where practical.
  - [x] **M3.4 — Request handling:** connect datagrams, generic-header parsing, M3.1 payload decoding, and the configured identity. **Done when:** valid broadcast and matching VIN/EID requests produce a correctly framed vehicle-identification response, while malformed or non-matching requests do not produce a response.
  - [x] **M3.5 — Integration and usage:** demonstrate the server and a client exchanging identification messages over localhost. **Done when:** an automated integration test verifies the UDP request/response and README instructions explain configuration, startup, and a discovery check.
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
