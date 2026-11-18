# m9_python_tester

## Goal and scope

M9 adds a standard-library Python client for exercising the local DoIP
simulator. It provides one scripted smoke-test command rather than a general
purpose UDS console. The client always connects to `127.0.0.1`; it does not
send arbitrary UDS requests or guess SecurityAccess keys. Its demo sends only
the simulator's documented deterministic demonstration key.

The demo covers routing activation, VIN and DID reads, session control,
rejected and authorized DID writes, the simulator's AirbagDeployment
SecurityAccess exchange, and a DTC count read. The fixed XOR seed/key transform
is simulation-only and is not suitable for real ECUs.

## DoIP client behavior

- Uses Python's standard library; no Python packages need to be installed.
- Validates the DoIP protocol-version preamble and enforces a 16 MiB receive
  payload limit.
- Reads complete frames from the TCP stream, independent of packet boundaries.
- Performs routing activation before diagnostic requests.
- Verifies the Diagnostic Message ACK and then reads the separate UDS response,
  checking payload types and logical addresses.
- Reports protocol, connection, timeout, and response errors with a nonzero
  exit code.

The tester accepts the simulator's TCP port and logical addresses as options.
It intentionally has no remote-host or arbitrary-request option.

## Running the tester

Start the server in one terminal, then run this from the repository root in
another:

```powershell
python .\tools\doip_tester.py demo
```

For a non-default local server port or logical address:

```powershell
python .\tools\doip_tester.py demo --port 13400 --tester-address 0x0E00 --ecu-address 0x1001
```

Run the end-to-end test against a built server with:

```powershell
ctest --preset debug --output-on-failure -R PythonDoipTesterIntegration
```

The integration test launches `doip_server` on loopback with OS-assigned TCP
and UDP ports, runs the Python demo, and exercises the Bootloader
SecurityAccess pair. The server supports `--bind-address`, `--tcp-port`, and
`--udp-port` options for this isolated test setup; port `0` requests an
available port. The existing positional configuration-path argument remains
supported.

See the [M9 tester demo](demo_m9_python_tester.md) for the complete walkthrough.
