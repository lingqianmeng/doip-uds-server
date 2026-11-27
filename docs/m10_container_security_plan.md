# m10_container_security_plan

## Status and goal

This plan tracks M10 deployment milestones: a native Debian package with a
`systemd` service, a Docker image, and automated CodeQL and Trivy scanning.
M10.3 is done for `amd64`; verification is recorded in that section. The
M10.2 and M10.4 sections describe their planned work and acceptance criteria.

## M10.1 - Define the boundary

See the [M10.1 deployment boundary](m10_1_deployment_boundary.md) for the
package, container, networking, and verification scope.

- Build and test natively on the target first, then package the executable,
  configuration, and `systemd` service as a `.deb`. Practice installation,
  automatic startup, logs, upgrades, and removal.
- Use a multi-stage Docker build: build and test in one stage, then copy the
  server and default ECU configuration into a small Debian-slim runtime image.
- Run the server as a non-root user and expose TCP and UDP port 13400. Keep
  configuration replaceable at runtime.
- Practice both deployments on the same target, ideally running 64-bit
  Raspberry Pi OS. A Pi 4 or an x86 Debian machine is also suitable; use the
  corresponding `arm64` or `amd64` artifacts.
- Build directly on the target first. Cross-compilation and Docker buildx/QEMU
  can be introduced later, rather than being prerequisites for learning the
  two deployment paths.
- Keep image signing and registry publishing out of scope unless explicitly
  added.
- On Linux, try host networking for DoIP broadcasts first, then bridge
  networking with published ports; document its discovery limitations. Run
  only one deployment at a time to avoid conflicts on port 13400.

## M10.2 - Native Debian deployment

- Reuse the graceful `SIGTERM` handling introduced in M10.3 for `systemd`
  stop and test orderly service shutdown.
- Add CMake install/package rules for a target-architecture `.deb` containing
  the server, administrator-editable default configuration, and `systemd`
  unit. Keep package install/upgrade from unexpectedly enabling or starting
  the service; run it as an unprivileged user.
- Add focused Linux CI coverage for package creation, installation, service
  lifecycle where `systemd` is available, config preservation on upgrade,
  removal/purge behavior, and the existing Python TCP diagnostic tester.
- Verify on the selected Linux hardware: install, start, logs, stop, upgrade,
  removal, and UDP discovery. Record architecture and results.

**Done when:** the package can be built and tested on the target architecture,
service lifecycle and config handling meet the boundary, TCP diagnostics pass,
and systemd shutdown is clean.

## M10.3 - Docker deployment

**Status:** Done for `amd64`. GitHub Actions CI builds the image and runs the
Docker smoke test. On the owner's work PC, WSL2 Ubuntu 24.04 with Docker
Engine, the bridge network with published TCP and UDP port 13400 served the
Python tester demo from Linux and from Windows, and `docker stop` shut the
container down gracefully. Since PR #43, a failed startup broadcast is
non-fatal: the server logs a `[VAM startup] Warning` and keeps running.
The [M10.3 Docker demo](demo_m10_3_docker.md) records those commands.

Raspberry Pi `arm64` builds and LAN broadcast validation on target hardware
remain follow-ups. Those checks are not claimed as done.

- Add graceful `SIGTERM` handling alongside `SIGINT`; Docker stop sends
  `SIGTERM` to the container's main process.
- Add a multi-stage `Dockerfile` and `.dockerignore`. Build and test in the
  builder stage; keep the runtime image small, use the explicit config path,
  and run as a non-root user.
- Support replacing the config at runtime, for example with a read-only bind
  mount. Document that config changes take effect after restart and simulator
  DID/DTC changes are not persisted.
- Add CI to build and run the image, exercise TCP diagnostics using the
  loopback-only Python tester, verify non-root execution and graceful
  `docker stop`, and check UDP request/reply.
- On Linux hardware, test host networking and bridge networking with published
  TCP/UDP ports. Separately verify LAN broadcasts and document platform limits;
  port publishing alone is not proof of broadcast discovery.

**Done when:** the image builds for the selected target architecture, runs
with the documented config and non-root identity, passes diagnostic and
shutdown checks, and its UDP/networking behavior is recorded. Those criteria
are met for `amd64`, with the hardware follow-ups above still open.

### M10.3 CI build issue: GCC 12 vector warnings

The Docker builder uses Debian Bookworm and GCC 12. During the optimized
Release build (`-O3`), GCC reported `-Warray-bounds` and
`-Wstringop-overflow` errors in libstdc++ while compiling test helpers that
assembled DoIP messages by appending payload ranges to `std::vector<uint8_t>`
with `vector::insert`. The project treats warnings as errors, so these
diagnostics stopped the image build even though the vector operations were
valid and no runtime bounds defect was identified.

The first failure was in `tests/unit/test_tcp_transport.cpp`. Reserving vector
capacity did not avoid the diagnostic; it moved to the next range insertion.
Changing that helper to allocate the complete message size and copy the header
and payload into their final ranges let compilation proceed, where the same
pattern surfaced in `tests/unit/test_vehicle_identification_handler.cpp` and
then in the TCP diagnostic-flow, diagnostic-message, and routing-activation
integration helpers. Each build stopped at its first failing translation
unit, which is why the failures appeared successively rather than together.

All affected test helpers now allocate the final size up front and use
`std::copy`. CI run `45502435451` passed the debug, release, and Docker
build/smoke-test jobs at commit `f2b4004`. This was a GCC 12 optimized-build
compatibility issue in test message construction, not a server runtime
failure.

## M10.4 - Security scanning and release documentation

- Add a CodeQL workflow for C++ and Python on pushes, pull requests, and a
  weekly schedule. Use a manual CMake build for C++ and minimal workflow
  permissions.
- Add Trivy scans for repository files and the built container image. Fail on
  fixable HIGH and CRITICAL findings and upload SARIF results to the GitHub
  Security tab where code scanning is available.
- Complete the deployment guide and demos for native and Docker use, including
  build/install/run/upgrade/removal commands, networking experiments, known
  limitations, and the security scan workflows. Update README and roadmap links.

**Done when:** both scanning workflows run on the intended triggers, findings
are gated and uploaded as specified, and the docs describe both deployment
paths and distinguish verified results from hardware-dependent checks.

## Completion criteria

- M10.3 is done for `amd64`. Raspberry Pi `arm64` and target-hardware LAN
  broadcast checks remain follow-ups. M10.2 and M10.4 are still open.
  Hardware-dependent results are not claimed until verified on the target.
