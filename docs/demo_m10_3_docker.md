# demo_m10_3_docker

This demo builds and runs the DoIP/UDS simulator as a non-root Docker
container, exercises TCP diagnostics and UDP vehicle identification, and
checks graceful shutdown. It uses the default configuration and binds the
published ports to loopback. Do not expose this simulator to a real vehicle
network.

## Prerequisites

- Docker Engine or Docker Desktop with Linux containers.
- CMake is not required on the host; the image build installs its build
  dependencies and runs the CMake/CTest suite in the builder stage.
- Run the commands from the repository root.

## Build and run

```powershell
docker build -t doip-uds-server:m10 .
docker run --detach --name doip-uds `
  --publish 127.0.0.1:13400:13400/tcp `
  --publish 127.0.0.1:13400:13400/udp `
  doip-uds-server:m10
```

Confirm the container runs as a non-root user and that it is listening:

```powershell
docker inspect --format '{{.Config.User}}' doip-uds
docker logs doip-uds
```

The reported user is `65532:65532`. The startup log lists the TCP and UDP
ports. The image includes `/etc/doip-uds-server/ecu_config.json`; the server
receives that explicit path as its startup configuration.

## Exercise TCP diagnostics and UDP discovery

Run the local-only Python tester against the forwarded TCP port:

```powershell
python .\tools\doip_tester.py demo --port 13400
```

Send a unicast UDP vehicle-identification request through the published UDP
port:

```powershell
python .\tests\integration\test_doip_udp_smoke.py --port 13400
```

Port publishing allows this unicast request/reply check. It does not guarantee
that LAN broadcasts reach the container or that its startup broadcasts reach
the LAN. On Linux, compare host networking with bridge mode and port
publishing, then check broadcasts from a separate LAN peer. Docker Desktop
networking differs from native Linux Docker; record actual results instead of
assuming broadcast support.

Startup announcements are best effort. If the host rejects the limited
broadcast, for example `sendto: Operation not permitted` with
`docker run --network host` on WSL2, the server logs a `[VAM startup] Warning`
and keeps serving TCP diagnostics and UDP identification requests. A refused
identification response is logged the same way. Failure to bind a port or to
load configuration still stops the process. Log lines are flushed so
`docker logs` shows `[TCP]`, `[UDS]`, and `[VAM]` output without allocating a
TTY.

To supply a different ECU configuration, mount a readable JSON file at the
container path and recreate the container:

```powershell
docker run --detach --name doip-uds-custom `
  --publish 127.0.0.1:13400:13400/tcp `
  --publish 127.0.0.1:13400:13400/udp `
  --mount "type=bind,source=$((Resolve-Path .\config\ecu_config.json).Path),target=/etc/doip-uds-server/ecu_config.json,readonly" `
  doip-uds-server:m10
```

The mounted configuration must be readable by the container's numeric
unprivileged user. Configuration changes require recreating or restarting the
container. Runtime DID writes and DTC clears are in-memory simulator state and
are not persisted.

## Verify graceful shutdown

```powershell
docker stop --time 5 doip-uds
docker logs doip-uds
docker inspect --format '{{.State.ExitCode}}' doip-uds
docker rm doip-uds
```

The logs should include `DoIP server stopped.` and the container exit code
should be `0`. The existing CTest suite also includes a Linux `SIGTERM`
shutdown check. Docker smoke tests run in the GitHub Actions CI workflow.

## CI build note

The Debian Bookworm builder uses GCC 12 with optimized builds and warnings
treated as errors. GCC diagnosed some valid `std::vector::insert` patterns in
test-only DoIP message helpers as bounds warnings inside libstdc++; those
helpers now allocate the final message size and copy into place. The resolved
incident and CI verification are recorded in the
[M10 plan](m10_container_security_plan.md#m103-ci-build-issue-gcc-12-vector-warnings).
