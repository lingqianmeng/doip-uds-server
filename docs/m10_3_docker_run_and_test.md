# m10_3_docker_run_and_test

This guide builds the DoIP/UDS simulator image and runs it on Docker's
default bridge network. Linux (native, or WSL2 on Windows) and Docker Engine
are already installed, and `docker run hello-world` succeeds. Run the
commands from the repository root. Do not expose this simulator to a real
vehicle network.

The [M10.3 Docker demo](demo_m10_3_docker.md) publishes port 13400 on
loopback and uses PowerShell. The commands here publish TCP and UDP 13400 on
the host, then call the loopback-only Python tester.

## Build the image

The `Dockerfile` is a multi-stage build. The `build` stage compiles a Release
binary with `-DDOIP_BUILD_TESTS=ON` and runs
`ctest --test-dir /build --output-on-failure`. If CTest fails, that `RUN`
fails and the build stops before the runtime image is produced. The
`runtime` stage copies `doip_server` and `config/ecu_config.json` into a
Debian Bookworm slim image, switches to user `65532:65532`, and starts the
server with `--config /etc/doip-uds-server/ecu_config.json`.

```sh
docker build -t doip-uds-server:m10 .
```

To rebuild from an empty cache, remove the old container and image first.
`docker rmi` fails while a container still references the image, with
`conflict: unable to delete doip-uds-server:m10`. `docker builder prune -f`
is optional. It deletes unused build cache and does not remove the tagged
image.

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

## Run on the bridge network

`-d` runs the container in the background on the default bridge network.
`-p` publishes container port 13400 for TCP and for UDP.
Remove an existing `doip-uds` container first; the
[lifecycle](#container-lifecycle) section shows the name conflict.

```sh
docker run -d --name doip-uds -p 13400:13400/tcp -p 13400:13400/udp doip-uds-server:m10
```

The entrypoint is `/usr/local/bin/doip_server`. The image command is
`--config /etc/doip-uds-server/ecu_config.json`, the file copied into the
image. Arguments after the image name replace that command.

To supply another configuration, mount a JSON file over the default path.
The file must be readable by user `65532`. The repository sample
`config/ecu_config.json` is readable this way. The tester command in this
guide expects logical address `0x1001`, which is the address in that sample.

```sh
docker run -d --name doip-uds \
  -p 13400:13400/tcp \
  -p 13400:13400/udp \
  -v "$(pwd)/config/ecu_config.json:/etc/doip-uds-server/ecu_config.json:ro" \
  doip-uds-server:m10
```

To mount the file somewhere else in the container, pass `--config` with the
container path. Those arguments replace the default command:

```sh
docker run -d --name doip-uds \
  -p 13400:13400/tcp \
  -p 13400:13400/udp \
  -v "$(pwd)/config/ecu_config.json:/config/ecu_config.json:ro" \
  doip-uds-server:m10 \
  --config /config/ecu_config.json
```

DID writes and DTC clears stay in process memory. The next start reads the
JSON file again. A changed mounted file is loaded on that start.

## Check the container

`docker run -d` prints a container id and returns. The server log is in
`docker logs`. The first announcement waits up to 500 ms, and the next two
follow at 500 ms intervals, so the three `[VAM startup] Sent` lines can take
up to about 1.5 seconds.

```sh
docker ps
docker logs doip-uds
docker port doip-uds
```

`docker ps` lists `doip-uds`, image `doip-uds-server:m10`, and host port
13400 published for TCP and UDP. `docker port` prints:

```text
13400/tcp -> 0.0.0.0:13400
13400/tcp -> [::]:13400
13400/udp -> 0.0.0.0:13400
13400/udp -> [::]:13400
```

A daemon without IPv6 prints the `0.0.0.0` lines only. With the default
configuration, `docker logs` shows:

```text
doip_server 0.1.0 loaded ECU identity at logical address 0x1001
[UDS] Configured synthetic DTCs: DTC_RAM_TEST DTC_RAM_ECC DTC_CONTROL_MODULE DTC_ROM_CHECK DTC_MEMORY_PROTECTION DTC_CONTROLLER_SUPERVISION DTC_NVM_OPERATION DTC_ADC_CHECK DTC_POWER_MONITORING DTC_SBC_CHECK
Listening for DoIP vehicle-identification requests on UDP port 13400 and TCP connections on port 13400. Press Ctrl+C to stop.
[VAM startup] Sent 1/3 to 255.255.255.255:13400 VIN=WVWZZZ00000000001
[VAM startup] Sent 2/3 to 255.255.255.255:13400 VIN=WVWZZZ00000000001
[VAM startup] Sent 3/3 to 255.255.255.255:13400 VIN=WVWZZZ00000000001
```

The listening line is the server's normal text. With `-d`, stop the
container with `docker stop`, which sends `SIGTERM`. Each `Sent` line means
that announcement's `sendto` to `255.255.255.255:13400` succeeded on the
container network. Whether another machine can capture the broadcast is a
separate question; see [Networking](#networking).

## Test with the Python tester

`tools/doip_tester.py` always connects to `127.0.0.1`. The demo command
passes `--port`; port `13400` is also the default. From the repository root
on Linux:

```sh
python3 tools/doip_tester.py demo --port 13400
```

On WSL2, Windows localhost TCP is forwarded into the Linux VM, so the same
check runs from Windows PowerShell in the repository root:

```powershell
python tools\doip_tester.py demo --port 13400
```

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

## Container lifecycle

`docker stop doip-uds` sends `SIGTERM`. The image sets `STOPSIGNAL SIGTERM`,
and the server handles `SIGTERM` and `SIGINT` with the same shutdown. A clean
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

Start the existing container with `docker start doip-uds`, or remove it and
create another one:

```sh
docker rm -f doip-uds
docker run -d --name doip-uds -p 13400:13400/tcp -p 13400:13400/udp doip-uds-server:m10
```

## Networking

With `-p`, TCP to `127.0.0.1:13400` on the Docker host reaches the server.
That is the address the Python tester uses. On WSL2, the PowerShell command
above reaches the published port because WSL2 forwards localhost TCP into
the Linux VM.

Publishing `13400/udp` does not carry broadcast discovery from Windows into
WSL2 or into the bridge network. A Windows datagram to
`255.255.255.255:13400` does not arrive as a vehicle-identification request
inside the container. Startup announcements are sent to that same limited
broadcast address on the container network. TCP to a known host IP still
reaches the published port.

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
Failure to bind a port, or to load the configuration, is still fatal.

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
[Run on the bridge network](#run-on-the-bridge-network). `docker start`
keeps the image the current container was created from. Use the
`--no-cache` sequence in [Build the image](#build-the-image) when cached
layers should be discarded.

## Command reference

| Command | Purpose |
|---|---|
| `docker build -t doip-uds-server:m10 .` | Build the image. CTest in the build stage fails the build when tests fail. |
| `docker build --no-cache -t doip-uds-server:m10 .` | Rebuild without using the build cache. |
| `docker rm -f doip-uds` | Remove the container if it is running or stopped. |
| `docker rmi doip-uds-server:m10` | Remove the image after no container references it. |
| `docker builder prune -f` | Delete unused build cache without prompting. |
| `docker run -d --name doip-uds -p 13400:13400/tcp -p 13400:13400/udp doip-uds-server:m10` | Start on the default bridge and publish port 13400. |
| `docker ps` | List running containers. |
| `docker ps -a` | List running and stopped containers. |
| `docker logs doip-uds` | Show the server log. |
| `docker port doip-uds` | Show published TCP and UDP port 13400. |
| `python3 tools/doip_tester.py demo --port 13400` | Run the demo from Linux against `127.0.0.1`. |
| `python tools\doip_tester.py demo --port 13400` | Run the same demo from Windows PowerShell on WSL2. |
| `docker stop doip-uds` | Send `SIGTERM` and leave the container stopped. |
| `docker start doip-uds` | Start the existing container again. |
