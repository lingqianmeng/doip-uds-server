# m10_1_deployment_boundary

## Status and scope

M10.1 defines the intended deployment boundary. The package, image, service,
and security workflows are split across M10.2 (native Debian), M10.3
(Docker), and M10.4 (security scanning and release documentation). Both
deployments use the same simulator source and configuration format. The
[M10 plan](m10_container_security_plan.md) records the implementation
sequence, status, and completion criteria.

The initial target is a 64-bit Linux system: Raspberry Pi OS on an ARM64 Pi
or Debian on an AMD64 machine. Build and test directly on the target first;
cross-compilation and emulated multi-platform builds are not prerequisites.
Use an isolated development network, not a real vehicle network. The
simulator's deterministic SecurityAccess is not production security.

## Existing behavior and implementation gap

The current CMake build produces `doip_server` and runs C++ unit and
integration tests and the Python tester integration through CTest. CMake does
not yet define an installation or Debian package. The existing CI runs debug
and release builds on Ubuntu; it does not build either deployment artifact.

The server loads ECU identity, DID initial values, and synthetic DTCs from a
JSON file at startup. By default the path is relative to the working directory.
The deployment artifacts must provide and explicitly select a readable config
file. Invalid or missing configuration must remain a visible startup failure,
not silently fall back to another file.

## Native deployment

- Build the C++17 server and run the existing tests on the target. Package the
  executable, default ECU configuration, and a `systemd` unit in a `.deb`
  built for the target architecture. Include required runtime dependencies;
  build tools and test dependencies are not runtime requirements.
- Use an explicit absolute configuration path in the service, rather than
  relying on the current working directory. Install the configuration as an
  administrator-editable Debian configuration file, preserving local changes
  on upgrade using Debian conffile behavior.
- Run the service as a dedicated unprivileged user. The service should not
  require write access to its configuration. Log to standard output and
  standard error for inspection through the journal.
- Make starting and enabling the service an explicit administrator action,
  so installation alone does not unexpectedly expose a network listener.
  An upgrade must not unexpectedly start or enable a previously stopped or
  disabled service.
  Document install, start, enable, status, logs, stop, upgrade, removal, and
  purge. Normal removal retains configuration; purge removes package-managed
  configuration.
- Changes to DIDs and DTCs are process-local simulator state, not persistent
  deployment data. Restarting reloads the configured initial values.

## Container deployment

- Use a multi-stage build: compile and run tests in a build stage, then copy
  the executable and default configuration into a Debian-slim runtime image
  with its required runtime libraries. Exclude build output, captures, the
  local PDF, and other nonessential files from the build context.
- Run as a non-root user and use an explicit configuration path. Allow an
  operator to replace the configuration at runtime, for example with a
  read-only bind mount. The container does not persist DID or DTC changes.
  The mounted file must be readable by the container user; the guide must
  explain that changing it takes effect after a restart.
- On the target, build and run the image for its architecture. Image signing
  and publishing to a registry are outside M10.

## Networking and shutdown

The current server defaults to `0.0.0.0`, TCP and UDP port `13400`, and a
configuration path relative to its working directory. It supports explicit
`--config`, `--bind-address`, `--tcp-port`, and `--udp-port` arguments.
Deployment must pass an explicit configuration path; choose a bind address and
firewall appropriate to the isolated test network. Do not run both deployment
variants on the same address and ports at once.

For Linux Docker, first exercise host networking so discovery uses the host
network interfaces. Also try bridge networking with TCP and UDP port mappings.
Port publishing can support TCP and unicast UDP, but does not guarantee that
LAN broadcasts reach the container or that its startup broadcasts reach the
LAN. Check discovery from another machine and document observed platform and
network limitations rather than claiming bridge broadcast support.
The server sends startup announcements to `255.255.255.255:13400` and replies
to valid vehicle-identification requests. A refused broadcast or response send
is logged and is not a startup failure. Test announcements and request/reply
separately. An isolated CI unicast check is useful, but is not evidence that
broadcast discovery works across a LAN.

The server handles both `SIGINT` and `SIGTERM`; graceful `SIGTERM` shutdown
is covered by the M10.3 Linux test and Docker smoke test. M10.2 should verify
that `systemd` stop uses the same graceful shutdown behavior.

## CI, security, and acceptance

- Keep the existing CMake/CTest suite and add native package and image builds
  and smoke tests in CI. Install the package in a suitable isolated Linux
  environment, exercise the service lifecycle where `systemd` is available,
  and verify configuration upgrade/removal behavior. Verify the chosen target
  architecture on suitable hardware or runners; report which architectures
  were not tested rather than treating an AMD64 build as ARM64 verification.
- Run the existing loopback-only Python tester against each locally reachable
  deployment for TCP diagnostics. Check UDP discovery separately, including a
  real LAN peer for broadcast behavior; the Python tester does not test UDP.
  Verify clean shutdown and port release for both variants.
- The Docker CI smoke test should verify non-root execution, loopback TCP
  diagnostics, UDP unicast request/reply, and clean SIGTERM shutdown. It does
  not replace ARM64, systemd, or LAN broadcast checks.
- Run CodeQL for C++ and Python on pushes, pull requests, and a weekly
  schedule, using a manual CMake build for C++. Run Trivy against repository
  files and the image; fail for fixable HIGH and CRITICAL findings. Upload
  SARIF even when a scan finds issues, where GitHub code scanning is available.
  Use the workflow permissions needed for checkout and security-result upload.
- Document commands and observed results for both deployments, including
  network limitations. Hardware-dependent checks remain outstanding until
  actually run on the target.

## M10.2 verification checklist

Record the operating system, architecture, network mode, commands, and outcome
for each applicable check. The package and image must be built from the same
revision. Use the default configuration for the existing Python tester, or
provide compatible logical-address options.

| Area | Expected observation |
|---|---|
| Build | CMake/CTest passes; the `.deb` and image build for the tested architecture. |
| Native lifecycle | A fresh install does not start or enable the service; an explicit start runs as the dedicated non-root user and diagnostics succeed. Stop releases the ports. |
| Native upgrade/removal | A locally edited config survives upgrade and normal removal; purge removes the package-managed config. The upgrade does not start or enable a service that was stopped or disabled. |
| Container lifecycle | The default image starts as non-root, diagnostics succeed, and stop terminates gracefully without a forced kill and releases published ports. |
| Configuration | Both deployments load an explicitly selected custom config after restart; missing or invalid config causes a visible startup failure. |
| UDP | Verify a vehicle-identification request and response for each deployment; capture startup announcements and test LAN broadcast discovery from a separate peer for native and host networking. Record bridge behavior separately. |
| CI and security | CI builds and smoke-tests both artifacts, CodeQL runs for both languages, and Trivy reports and gates findings according to the policy above. SARIF upload is attempted where supported. |

Use the existing loopback-only tester on the deployment host for TCP; a remote
machine cannot use it to test the Pi over the network without changing the
tester. A LAN peer is still needed for the separate broadcast checks. If
target hardware, `systemd`, or GitHub code scanning is unavailable, record
the corresponding checks as outstanding rather than reporting them as passed.
