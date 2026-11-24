# m10_container_security_plan

## Status and goal

This is the proposed plan for M10, not a description of implemented
functionality. M10 will practice two deployments from the same source:
a native Debian package with a `systemd` service and a Docker image. It also
adds automated CodeQL and Trivy scanning.

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

- Add graceful `SIGTERM` handling alongside `SIGINT` and test orderly shutdown.
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
and shutdown is clean.

## M10.3 - Docker deployment

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
shutdown checks, and its UDP/networking behavior is recorded.

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

- The M10.2, M10.3, and M10.4 done criteria are met and their results are
  documented. Hardware-dependent results are not claimed until verified on
  the target.
