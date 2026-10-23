# doip-uds-server

[![CI](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml/badge.svg)](https://github.com/lingqianmeng/doip-uds-server/actions/workflows/ci.yml)

A C++17 project for building a simulated ECU diagnostic server using DoIP (ISO 13400-2) and UDS (ISO 14229-1), with Raspberry Pi as a target platform.

> **Current status:** project scaffold (Milestone 1). The server currently prints its version; DoIP networking and UDS services are not implemented yet.

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

The key current interfaces are `doip_uds::version()` in `include/doip_uds/version.hpp`, its implementation in `src/version.cpp`, and the executable `main()` in `apps/doip_server/main.cpp`. These are scaffold-level interfaces; no network listener, protocol codec, or UDS dispatcher exists yet.

## Development direction

Implement protocol logic in the core library, keeping the executable entry point focused on startup and wiring. Put public interfaces in `include/doip_uds/`, implementations in `src/`, and focused unit tests in `tests/unit/`. Add integration tests when network behavior is introduced. The sample `config/ecu_config.json` is not consumed yet; configuration loading should be added explicitly in a later milestone.

## Roadmap

- [x] M1 Project setup, CI
- [ ] M2 DoIP header codec
- [ ] M3 UDP vehicle identification
- [ ] M4 TCP routing activation and diagnostic messages
- [ ] M5 UDS dispatcher, 0x10 / 0x3E / 0x11
- [ ] M6 0x22 / 0x2E
- [ ] M7 0x19 / 0x14
- [ ] M8 0x27 SecurityAccess
- [ ] M9 Python tester and integration tests
- [ ] M10 Docker, CodeQL, Trivy
