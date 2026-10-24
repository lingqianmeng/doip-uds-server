# doip-uds-server

[![CI](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml/badge.svg)](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml)

A C++17 project for building a simulated ECU diagnostic server using DoIP (ISO 13400-2) and UDS (ISO 14229-1), with Raspberry Pi as a target platform.

> **Current status:** M2 complete; M3.1 vehicle-identification payload codec cross-checked against ISO 13400-2:2012(E). UDP discovery and UDS services are not implemented yet.

## Project layout

```text
apps/doip_server/       Executable entry point
config/                 ECU configuration example (not yet loaded by the server)
include/doip_uds/       Public headers
src/                    Core library implementation
tests/unit/             GoogleTest unit tests
CMakeLists.txt          Targets, compiler settings, and dependency setup
CMakePresets.json       Debug/release Ninja build presets
```

## Build and test

Requirements: CMake 3.21 or newer, Ninja, and a C++17-capable compiler. CMake selects the compiler available in the environment; the presets do not force GCC, MSVC, or a vcpkg triplet.

GoogleTest is used for unit tests. CMake uses an installed package if available; otherwise it downloads GoogleTest with `FetchContent` during configuration. The fallback therefore requires network access on the first configure. **vcpkg is not used** by the current build.

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use `release` instead of `debug` for a release build. Presets generate build files under `build/debug` and `build/release`.

Run the executable:

```sh
# Linux
./build/debug/doip_server

# Windows PowerShell
.\build\debug\doip_server.exe
```

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
4. With `DOIP_BUILD_TESTS=ON` (the default), it finds or downloads GoogleTest, creates `unit_tests` from `tests/unit/`, and registers the tests with CTest.
5. Ninja executes the generated compile and link steps. CTest runs the registered tests.

The core library currently exposes `doip_uds::version()`, the DoIP generic-header codec (`DoipHeader`, `encode_doip_header()`, and `decode_doip_header()`), and vehicle-identification request/response payload codecs under `include/doip_uds/`. These codecs do not handle network I/O. The executable entry point is `main()` in `apps/doip_server/main.cpp`; it does not start a listener yet. The M3.1 payload type assignments and field layouts are cross-checked against the project's local reference copy of ISO 13400-2:2012(E); this does not imply conformance to later editions or complete protocol conformance.

## Development direction

Implement protocol logic in the core library, keeping the executable entry point focused on startup and wiring. Put public interfaces in `include/doip_uds/`, implementations in `src/`, and focused unit tests in `tests/unit/`. Add integration tests when network behavior is introduced. The sample `config/ecu_config.json` is not consumed yet; configuration loading should be added explicitly in a later milestone.

## Roadmap

- [x] M1 Project setup, CI
- [x] M2 DoIP header codec
- [ ] M3 UDP vehicle identification
  - [x] **M3.1 — Payload codec:** represent broadcast, VIN-targeted, and EID-targeted identification requests and the identification response; encode/decode their payload bytes. **Done when:** field order, byte order, payload types, and exact sizes are covered by unit tests.
  - [ ] **M3.2 — ECU identity configuration:** load the ECU's VIN, logical address, EID, and GID from `config/ecu_config.json`. **Done when:** valid configuration produces a typed identity and invalid/missing fields produce a clear startup error; validation has unit tests.
  - [ ] **M3.3 — UDP transport:** create a cross-platform UDP listener on the DoIP port (13400), separate from protocol handling. **Done when:** it can bind, receive datagrams, report bind/receive errors, and shut down cleanly; transport behavior has tests where practical.
  - [ ] **M3.4 — Request handling:** connect datagrams, generic-header parsing, M3.1 payload decoding, and the configured identity. **Done when:** valid broadcast and matching VIN/EID requests produce a correctly framed vehicle-identification response, while malformed or non-matching requests do not produce a response.
  - [ ] **M3.5 — Integration and usage:** demonstrate the server and a client exchanging identification messages over localhost. **Done when:** an automated integration test verifies the UDP request/response and README instructions explain configuration, startup, and a discovery check.
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
