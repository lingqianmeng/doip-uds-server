# Vehicle-identification payload codec (M3.1)

M3.1 adds C++ types and codecs for DoIP vehicle-identification requests and
responses. It converts between typed values and the payload bytes carried after
the 8-byte DoIP generic header. It does not implement configuration loading,
UDP networking, or request handling.

## Request types

Vehicle-identification requests are distinguished by the DoIP generic-header
payload type. The broadcast request has no payload; the targeted forms carry
either a VIN or an EID:

| Payload type | Value | Payload |
|---|---:|---|
| Vehicle-identification request | `0x0001` | Empty |
| Vehicle-identification request by VIN | `0x0002` | 17 VIN bytes |
| Vehicle-identification request by EID | `0x0003` | 6 EID bytes |

For example, a request by EID uses payload type `0x0003` in the generic header
and exactly six EID bytes after that header. The payload type is not repeated
inside the request payload.

The C++ API models the alternatives with `VehicleIdentificationRequestMessage`,
a `std::variant` containing `VehicleIdentificationRequest`,
`VehicleIdentificationRequestByVin`, or
`VehicleIdentificationRequestByEid`. Encoding returns both the payload type
and payload bytes because both are needed to construct the full DoIP message.

## Response format

A vehicle-identification response uses payload type `0x0004`. Its payload is
exactly 33 bytes:

| Payload offset | Size | Field |
|---:|---:|---|
| 0 | 17 bytes | VIN |
| 17 | 2 bytes | Logical address, big-endian |
| 19 | 6 bytes | EID |
| 25 | 6 bytes | GID |
| 31 | 1 byte | Further-action code |
| 32 | 1 byte | Synchronization status |

The total is `17 + 2 + 6 + 6 + 1 + 1 = 33` bytes. Multi-byte fields use
network byte order (big-endian), so logical address `0x0E80` is encoded as
`0E 80`.

The response type in the code stores VIN, EID, and GID as fixed-size byte
arrays. This makes their wire lengths explicit and avoids treating binary
identifier fields as variable-length strings.

## Implementation

- `include/doip_uds/vehicle_identification.hpp` declares payload-type constants,
  request/response structures, and codec functions.
- `src/vehicle_identification.cpp` maps request variants to payload types and
  bytes, validates request type and exact payload sizes when decoding, and
  encodes/decodes the fixed response layout.
- `tests/unit/test_vehicle_identification.cpp` tests request encoding and
  decoding, response byte layout and round-trip behavior, and invalid request
  types or payload sizes.
- The top-level and test `CMakeLists.txt` files include the implementation and
  unit tests in their build targets.

Request decoding and response decoding return `std::optional`: a value means
the type and size were accepted; `std::nullopt` means the payload type is
unknown, the pointer is invalid, or the payload has the wrong exact size.
The broadcast request is the special case where an empty payload is valid,
including a null pointer with size zero.

## Scope and limitations

M3.1 handles payload data only. It does **not** encode or decode the generic
DoIP header; use the M2 header codec for that. It also does not:

- validate whether VIN bytes are valid VIN characters;
- validate semantic values for the further-action or synchronization fields;
- load ECU identity from `config/ecu_config.json`;
- decide whether a targeted request matches this ECU;
- receive or send UDP datagrams.

Those responsibilities are planned for later M3 steps: identity loading in
M3.2, UDP transport in M3.3, request matching and response handling in M3.4,
and end-to-end validation in M3.5.
