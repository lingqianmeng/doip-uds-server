# m10_container_security_plan

## Status and goal

This is the proposed plan for M10, not a description of implemented
functionality. M10 will practice two deployments from the same source:
a native Debian package with a `systemd` service and a Docker image. It also
adds automated CodeQL and Trivy scanning.

## M10.1 - Define the boundary

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

## M10.2 - Implementation

1. Build on the target and verify TCP diagnostics and UDP discovery. Create a
   native `.deb` with the executable, configuration, and a `systemd` service.
   Exercise installation, startup, logs, upgrades, and removal.
2. Add a `Dockerfile` and `.dockerignore`, excluding build directories,
   captures, and the local PDF from the build context.
3. Add graceful `SIGTERM` handling alongside `SIGINT` so `docker stop` and
   `systemd` can
   shut down the server cleanly. Test the shutdown behavior.
4. Add CI builds and smoke tests for the native package and image. For the
   image, run the existing loopback-only Python tester against the published
   TCP port. Verify UDP discovery separately and document platform-dependent
   broadcast limits.
5. Add a CodeQL workflow for C++ and Python on pushes, pull requests, and a
   weekly schedule, using a manual CMake build for C++.
6. Add a Trivy workflow to scan the image and repository files. Fail on
   fixable HIGH and CRITICAL findings and upload SARIF results to the GitHub
   Security tab.
7. Document and demonstrate both deployment paths, networking, and security
   scanning; update the README and roadmap documentation map.

## Completion criteria

- The native package installs on the target, starts with its default
  configuration under `systemd`, and supports the documented upgrade and
  removal procedures.
- The image builds for the target architecture; the server starts with its
  default configuration and runs as a non-root user.
- Existing tests and native and container TCP diagnostic smoke tests pass. UDP
  discovery and clean shutdown are verified for both deployment paths.
- CodeQL and Trivy workflows run with the intended permissions and triggers;
  scan results are visible in GitHub where code scanning is available.
- The documentation and demo explain building, running, configuring,
  networking, and scanning both deployment paths, including known limitations.
