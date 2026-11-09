# Vehicle-identification request handler (M3.4)

M3.4 connects one received DoIP UDP datagram to the existing generic-header
codec, vehicle-identification payload codec, and configured ECU identity. It is
a pure protocol function: it performs no socket I/O and does not start the
server. M3.5 will connect this handler to the UDP listener and verify a complete
localhost exchange.

## Handler behavior

`handle_vehicle_identification_datagram()` accepts an `EcuIdentity` and a
datagram's bytes. It returns a complete response datagram when the request is
valid and targets this ECU; otherwise it returns `std::nullopt`.

The handler checks, in order:

1. The datagram contains a complete, decodable 8-byte DoIP generic header.
2. The header uses protocol version `0xFF`, the default version for
   vehicle-identification requests in ISO 13400-2:2012(E).
3. The declared payload length exactly matches the bytes following the header.
4. The payload type and size decode as a broadcast, VIN-targeted, or
   EID-targeted vehicle-identification request.
5. A VIN- or EID-targeted request matches this ECU's configured identity.
   Broadcast requests match every ECU.

For a match, the handler builds the 33-byte identification response payload
from the configured VIN, logical address, EID, and GID. It sets further action
and synchronization status to `0x00`, then wraps the payload with a generic
header using protocol version `0x02`, response payload type `0x0004`, and the
exact payload length.

Malformed headers, unsupported versions or payload types, incorrect lengths,
and non-matching VIN/EID values produce no response. This function does not
send the response; its caller will do that when socket response support is
added.

## Implementation and tests

- `include/doip_uds/vehicle_identification_handler.hpp` declares the handler.
- `src/vehicle_identification_handler.cpp` composes the existing codecs and
  identity data to validate requests and construct framed responses.
- `tests/unit/test_vehicle_identification_handler.cpp` covers broadcast and
  matching targeted requests, identity mismatches, malformed datagrams, and
  response header and identity fields.

Run the focused tests from the repository root:

```sh
cmake --build --preset debug
./build/debug/tests/unit_tests --gtest_filter=VehicleIdentificationHandler.*
```

On Windows PowerShell, run
`.\build\debug\tests\unit_tests.exe --gtest_filter=VehicleIdentificationHandler.*`.

This milestone only tests request handling in memory. It does not claim
end-to-end network behavior or full DoIP conformance; those are outside M3.4.
