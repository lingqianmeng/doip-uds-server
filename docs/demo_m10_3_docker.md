# demo_m10_3_docker

This demo builds and runs the DoIP/UDS simulator as a non-root Docker
container, exercises TCP diagnostics and UDP vehicle identification, and
checks graceful shutdown. It uses the default configuration. The PowerShell
commands publish port 13400 on loopback. The Linux commands publish it on the
default bridge network. Linux (native or WSL2 on Windows) and Docker Engine
are already installed, and `docker run hello-world` succeeds. Do not expose
this simulator to a real vehicle network.

## Prerequisites

- Docker Engine or Docker Desktop with Linux containers.
  `docker run hello-world` already succeeds.
- CMake is not required on the host. The image build installs its build
  dependencies and runs the CMake/CTest suite in the builder stage. If CTest
  fails, that step fails and the runtime image is not produced.
- Run the commands from the repository root.

## Build and run

The `Dockerfile` is a multi-stage build. The `build` stage compiles a Release
binary with `-DDOIP_BUILD_TESTS=ON` and runs
`ctest --test-dir /build --output-on-failure`. The `runtime` stage copies
`doip_server` and `config/ecu_config.json` into a Debian Bookworm slim image,
switches to user `65532:65532`, and starts the server with
`--config /etc/doip-uds-server/ecu_config.json`. The entrypoint is
`/usr/local/bin/doip_server`. Arguments after the image name replace that
command.

```powershell
docker build -t doip-uds-server:m10 .
docker run --detach --name doip-uds `
  --publish 127.0.0.1:13400:13400/tcp `
  --publish 127.0.0.1:13400:13400/udp `
  doip-uds-server:m10
```

On Linux, the same image can listen on the default bridge network. `-d` runs
it in the background, and `-p` publishes container port 13400 for TCP and UDP
on the host. Remove an existing `doip-uds` container first; see
[Container lifecycle](#container-lifecycle).

```sh
docker run -d --name doip-uds -p 13400:13400/tcp -p 13400:13400/udp doip-uds-server:m10
```

To rebuild from an empty cache, remove the container and the image first.
`docker rmi` fails while a container still references the image, with
`conflict: unable to delete doip-uds-server:m10`. `docker builder prune -f`
is optional: it deletes unused build cache and leaves the tagged image in
place until `docker rmi`.

```sh
docker rm -f doip-uds
docker rmi doip-uds-server:m10
docker builder prune -f
docker build --no-cache -t doip-uds-server:m10 .
```

A later source change is picked up by a normal
`docker build -t doip-uds-server:m10 .`, because the image copies the build
context. Create a new container to run that image. `docker start` runs the
image recorded on the existing container. `.dockerignore` excludes `docs/`,
so a documentation-only edit does not change the image.

Confirm the container runs as a non-root user and that it is listening:

```powershell
docker inspect --format '{{.Config.User}}' doip-uds
docker logs doip-uds
```

The reported user is `65532:65532`. The startup log lists the TCP and UDP
ports. The image includes `/etc/doip-uds-server/ecu_config.json`; the server
receives that explicit path as its startup configuration.

## Check the container

`docker run -d` prints a container id and returns. The server log is the
`docker logs` output from the previous section. The first announcement waits
up to 500 ms, and the next two follow at 500 ms intervals, so the three
`[VAM startup] Sent` lines can take up to about 1.5 seconds.

```sh
docker ps
docker port doip-uds
```

`docker ps` lists `doip-uds` and image `doip-uds-server:m10`. For the bridge
publish above, `docker port` prints:

```text
13400/tcp -> 0.0.0.0:13400
13400/tcp -> [::]:13400
13400/udp -> 0.0.0.0:13400
13400/udp -> [::]:13400
```

A daemon without IPv6 prints the `0.0.0.0` lines only. The loopback publish
binds host port 13400 on `127.0.0.1` instead. With the default configuration,
the log shows:

```text
doip_server 0.1.0 loaded ECU identity at logical address 0x1001
[UDS] Configured synthetic DTCs: DTC_RAM_TEST DTC_RAM_ECC DTC_CONTROL_MODULE DTC_ROM_CHECK DTC_MEMORY_PROTECTION DTC_CONTROLLER_SUPERVISION DTC_NVM_OPERATION DTC_ADC_CHECK DTC_POWER_MONITORING DTC_SBC_CHECK
Listening for DoIP vehicle-identification requests on UDP port 13400 and TCP connections on port 13400. Press Ctrl+C to stop.
[VAM startup] Sent 1/3 to 255.255.255.255:13400 VIN=WVWZZZ00000000001
[VAM startup] Sent 2/3 to 255.255.255.255:13400 VIN=WVWZZZ00000000001
[VAM startup] Sent 3/3 to 255.255.255.255:13400 VIN=WVWZZZ00000000001
```

The listening line is the server's normal text. With `-d`, stop the container
with `docker stop`, which sends `SIGTERM`. Each `Sent` line means that
announcement's `sendto` to `255.255.255.255:13400` succeeded on the container
network. Whether another machine can capture the broadcast is covered in
[Networking](#networking).

## Exercise TCP diagnostics and UDP discovery

`tools/doip_tester.py` always connects to `127.0.0.1`. `--port` selects the
local TCP port; `13400` is also the default. Run the tester against the
forwarded TCP port.

On Linux:

```sh
python3 tools/doip_tester.py demo --port 13400
```

On Windows PowerShell, from the repository root:

```powershell
python .\tools\doip_tester.py demo --port 13400
```

On WSL2, that PowerShell command reaches the published port because WSL2
forwards localhost TCP into the Linux VM.

A successful demo prints:

```text
Routing activation succeeded: tester 0x0E00, ECU 0x1001
VIN: WVWZZZ00000000001
Initial DID F1A0: 00 00 00 00
Reject write in default session: 7F 2E 7E
Enter extended session: 50 03 00 32 01 F4
Reject write before SecurityAccess: 7F 2E 33
Request AirbagDeployment seed: 67 5F 00 00 12 34
Send simulator key 0x0000B7F7: 67 60
Write DID F1A0: 6E F1 A0
DID F1A0 readback: 11 22 33 44
Reject write to read-only VIN: 7F 2E 31
Configured DTC count: 10
```

Routing activation uses tester address `0x0E00` and ECU address `0x1001`.
The VIN and the initial DID `F1A0` value `00 00 00 00` come from
`config/ecu_config.json`. A write in the default session is rejected with
NRC `7F 2E 7E`. After extended session, the same write is rejected with
`7F 2E 33` until SecurityAccess completes. On a new connection the
AirbagDeployment seed response is `67 5F 00 00 12 34`, and the simulator
demonstration key response is `67 60`. DID `F1A0` then reads back
`11 22 33 44`. A write to the read-only VIN is rejected with `7F 2E 31`.
The default configuration has ten synthetic DTCs, and the demo reports
`Configured DTC count: 10`.

The seed and key are simulator demonstration data for this fixed TCP
exchange. After the demo, `docker logs doip-uds` also contains `[TCP]` and
`[UDS]` lines for that connection. A failed run prints
`DoIP tester failed: ...` to standard error and exits `1`.

Send a unicast UDP vehicle-identification request through the published UDP
port:

```powershell
python .\tests\integration\test_doip_udp_smoke.py --port 13400
```

Port publishing allows this unicast request/reply check. Broadcast limits are
in [Networking](#networking).

To supply a different ECU configuration, mount a readable JSON file at the
container path and recreate the container:

```powershell
docker run --detach --name doip-uds-custom `
  --publish 127.0.0.1:13400:13400/tcp `
  --publish 127.0.0.1:13400:13400/udp `
  --mount "type=bind,source=$((Resolve-Path .\config\ecu_config.json).Path),target=/etc/doip-uds-server/ecu_config.json,readonly" `
  doip-uds-server:m10
```

On Linux, add
`-v "$(pwd)/config/ecu_config.json:/etc/doip-uds-server/ecu_config.json:ro"`
to the bridge `docker run` in [Build and run](#build-and-run). To mount the
file at a different container path, pass `--config` so it replaces the
default command:

```sh
docker run -d --name doip-uds \
  -p 13400:13400/tcp \
  -p 13400:13400/udp \
  -v "$(pwd)/config/ecu_config.json:/config/ecu_config.json:ro" \
  doip-uds-server:m10 \
  --config /config/ecu_config.json
```

The mounted configuration must be readable by user `65532`. The tester
command in this demo expects logical address `0x1001`, which is the address
in `config/ecu_config.json`. Configuration changes require recreating or
restarting the container. Runtime DID writes and DTC clears are in-memory
simulator state and are not persisted.

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

## Container lifecycle

`docker stop` sends `SIGTERM`. The image sets `STOPSIGNAL SIGTERM`, and the
server handles `SIGTERM` and `SIGINT` with the same shutdown. A clean
shutdown adds a blank line and then:

```text
DoIP server stopped.
```

The container exit code is `0`. That line is the orderly shutdown. A fatal
server error prints `DoIP server failed: ...` and the process exits `1`.

`docker ps` lists running containers. After a stop, `docker ps -a` shows
`doip-uds` as `Exited (0)`.

`docker start doip-uds` starts that same container. The process loads the
configuration again and sends three new announcements. In-memory DID and DTC
changes from the previous process are gone.

`docker run` creates a new container. When `doip-uds` already exists,
including while it is stopped, Docker rejects the name:

```text
docker: Error response from daemon: Conflict. The container name "/doip-uds" is already in use by container "<id>". You have to remove (or rename) that container to be able to reuse that name.
```

Start the existing container with `docker start doip-uds`, or remove it with
`docker rm -f doip-uds` and run the command from
[Build and run](#build-and-run) again.

## Networking

With `-p`, TCP to `127.0.0.1:13400` on the Docker host reaches the server.
That is the address the Python tester uses. On WSL2, Windows PowerShell can
use the same port because WSL2 forwards localhost TCP into the Linux VM.

Publishing `13400/udp` does not carry broadcast discovery from Windows into
WSL2 or into the bridge network. A Windows datagram to
`255.255.255.255:13400` does not arrive as a vehicle-identification request
inside the container. Startup announcements are sent to that same limited
broadcast address on the container network. TCP to a known host IP still
reaches the published port. Docker Desktop networking differs from native
Linux Docker; record actual results for a separate LAN peer.

`--network host` places the server in the host network namespace, where it
binds port 13400 on the host directly. On WSL2, `sendto` of
`255.255.255.255` can fail with `sendto: Operation not permitted`. Since
PR #43 (issue #42), each failed announcement is only a warning, and the
server keeps running:

```text
[VAM startup] Warning: announcement 1/3 failed: sendto: Operation not permitted
[VAM startup] Warning: announcement 2/3 failed: sendto: Operation not permitted
[VAM startup] Warning: announcement 3/3 failed: sendto: Operation not permitted
```

The server still waits 500 ms between those attempts. TCP diagnostics and
UDP identification requests continue. A refused identification response is
logged as `[VAM response] Warning: response to <address>:<port> failed: ...`
and the receive loop continues. An error that means the UDP socket itself is
unusable still stops the process with `DoIP server failed: ...`. On native
Linux, host networking can send the announcements and log `Sent` instead.
Failure to bind a port, or to load the configuration, is still fatal. Log
lines are flushed, so `docker logs` shows `[TCP]`, `[UDS]`, and `[VAM]`
output without allocating a TTY.

## Troubleshooting

When the tester prints `DoIP tester failed: Could not connect to the local
DoIP server on TCP port 13400:` (on Linux, often `Connection refused`) or
`DoIP tester failed: Timed out while waiting for a DoIP frame`, check the
container and its log:

```sh
docker ps -a
docker logs doip-uds
```

`docker ps` omits exited containers. `docker ps -a` includes them. A fatal
error after startup begins is `DoIP server failed: ...`. If port 13400 is
already taken in the container's network namespace, the log is:

```text
DoIP server failed: bind: Address already in use
```

The container exits `1` and never prints `Listening for DoIP`. A missing or
unreadable configuration fails earlier, still with exit code `1`:

```text
Failed to load ECU identity: Unable to open ECU configuration file '<path>'
```

If `docker run` reports `Bind for 0.0.0.0:13400 failed: port is already
allocated`, host port 13400 is already published. Stop the other publisher
and remove any container left in the `Created` state (`docker ps -a`, then
`docker rm -f`). To leave the other publisher running, map a free host port
and pass that port to the tester. The process inside the container still
listens on 13400:

```sh
docker run -d --name doip-uds -p 18400:13400/tcp -p 18400:13400/udp doip-uds-server:m10
python3 tools/doip_tester.py demo --port 18400
```

After a source change, run `docker build -t doip-uds-server:m10 .` again,
then `docker rm -f doip-uds` and the `docker run` command in
[Build and run](#build-and-run). `docker start` keeps the image the current
container was created from. Use the `--no-cache` sequence in that section
when cached layers should be discarded.

## CI build note

The Debian Bookworm builder uses GCC 12 with optimized builds and warnings
treated as errors. GCC diagnosed some valid `std::vector::insert` patterns in
test-only DoIP message helpers as bounds warnings inside libstdc++; those
helpers now allocate the final message size and copy into place. The resolved
incident and CI verification are recorded in the
[M10 plan](m10_container_security_plan.md#m103-ci-build-issue-gcc-12-vector-warnings).

## Command reference

| Command | Purpose |
|---|---|
| `docker build -t doip-uds-server:m10 .` | Build the image. CTest in the build stage fails the build when tests fail. |
| `docker build --no-cache -t doip-uds-server:m10 .` | Rebuild without using the build cache. |
| `docker rm -f doip-uds` | Remove the container if it is running or stopped. |
| `docker rmi doip-uds-server:m10` | Remove the image after no container references it. |
| `docker builder prune -f` | Delete unused build cache without prompting. |
| `docker run -d --name doip-uds -p 13400:13400/tcp -p 13400:13400/udp doip-uds-server:m10` | Start on the default bridge and publish port 13400. |
| `docker ps` / `docker ps -a` | List running containers, or running and stopped containers. |
| `docker logs doip-uds` | Show the server log. |
| `docker port doip-uds` | Show published TCP and UDP port 13400. |
| `python3 tools/doip_tester.py demo --port 13400` | Run the demo from Linux against `127.0.0.1`. |
| `python .\tools\doip_tester.py demo --port 13400` | Run the same demo from Windows PowerShell. |
| `docker stop --time 5 doip-uds` | Send `SIGTERM` and wait up to 5 seconds. |
| `docker start doip-uds` | Start the existing container again. |
