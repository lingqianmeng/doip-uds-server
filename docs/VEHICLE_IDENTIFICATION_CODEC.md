# Vehicle-identification payload codec (M3.1)

M3.1 adds C++ types and codecs for DoIP vehicle-identification requests and
responses. The codec converts between typed values and payload bytes that follow
the 8-byte DoIP generic header. It does not read or write sockets, load ECU
configuration, or decide how the server responds to a request.

## Where these bytes fit

A complete DoIP message has a generic header followed by the payload:

```text
+----------------------+-----------------------------+
| 8-byte generic header| payload (0 or more bytes)    |
+----------------------+-----------------------------+
```

The header codec from M2 encodes the protocol version, payload type, and payload
length. The M3.1 codec handles the vehicle-identification payload data and
returns the request's payload type alongside its payload bytes. A later server
step can combine those pieces to form the full message.

## Request formats in the implementation

The implementation represents three vehicle-identification request forms:

| Request | Payload type in ISO 13400-2:2012(E) | Payload bytes |
|---|---:|---|
| Broadcast vehicle-identification request | `0x0001` | Empty |
| Request by EID | `0x0002` | 6 EID bytes |
| Request by VIN | `0x0003` | 17 VIN bytes |

The payload type is carried in the generic header, not repeated in the payload.
Thus, an EID-targeted request consists of type `0x0002` in the header and six
EID bytes after the header. A VIN-targeted request uses type `0x0003` and 17
ASCII VIN bytes. In the 2012 edition, support for the EID-targeted request is
optional; support for the VIN-targeted request is mandatory.

`VehicleIdentificationRequestMessage` is a `std::variant` of the three request
structures. `encode_vehicle_identification_request()` inspects which variant
it receives and returns an `EncodedVehicleIdentificationRequest` containing
both the payload type and the corresponding payload bytes. The broadcast form
has an empty payload.

`decode_vehicle_identification_request()` takes the payload type and payload
bytes separately. It returns the matching variant only when the type is
recognized and the payload size is exact. The broadcast request is valid with
size zero, including a null data pointer when the size is zero.

## Response format

The vehicle-identification response uses payload type `0x0004`. Its payload is
33 bytes:

| Payload offset | Size | Field |
|---:|---:|---|
| 0 | 17 bytes | VIN |
| 17 | 2 bytes | Logical address, big-endian |
| 19 | 6 bytes | EID |
| 25 | 6 bytes | GID |
| 31 | 1 byte | Further-action value |
| 32 | 1 byte | Synchronization-status value |

The length is `17 + 2 + 6 + 6 + 1 + 1 = 33` bytes. The multi-byte logical
address uses network byte order (big-endian), so `0x1001` is encoded as `10 01`.
VIN, EID, and GID are fixed-size byte arrays in `VehicleIdentificationResponse`,
which makes their sizes explicit in the API.

`encode_vehicle_identification_response()` produces the fixed-layout byte
array. `decode_vehicle_identification_response()` accepts only a non-null
buffer of exactly 33 bytes and returns `std::nullopt` otherwise.

## Cross-check against ISO 13400-2:2012(E)

The payload types and field layouts were cross-checked against the supplied
local copy of **ISO 13400-2:2012(E)**:

- Clause 7.1.2, Table 12 (printed page 21) assigns `0x0001` to the broadcast
  identification request, `0x0002` to the request with EID, `0x0003` to the
  request with VIN, and `0x0004` to the response/announcement.
- Clause 7.1.4, Tables 16-18 (printed pages 25-26) specifies an empty broadcast
  request, a six-byte EID request, and a 17-byte ASCII VIN request.
- Clause 7.1.4, Table 19 (printed page 26) gives the response field offsets and
  sizes shown above. The 33-byte response layout matches that table.
- Clause 7.1.4, Tables 20-21 (printed page 27) defines further-action and
  VIN/GID synchronization-status values.
- Clause 7.1.2, Table 11 (printed page 20) defines `0xFF` (inverse `0x00`) as
  the default protocol-version value for vehicle-identification request
  messages. A DoIP entity must support and ignore this default value for those
  requests.
- Table 39 (printed page 49) reserves `0x0E00`-`0x0FFF` for external
  test-equipment logical addresses. Table 40 (printed page 59) defines
  all-zero and all-`0xFF` invalidity values for VIN, logical address, EID, and
  GID.

This verification is against the supplied 2012 edition only; it does not claim
compatibility with later editions or amendments. The codec keeps further-action
and synchronization-status fields as raw bytes. In the 2012 edition, further
action `0x00` means no further action is required, `0x10` indicates routing
activation for central security, and `0x11`-`0xFF` may be OEM-specific; `0x01`-
`0x0F` are reserved. Synchronization status `0x00` means VIN/GID are
synchronized and `0x10` means synchronization is incomplete; other values are
reserved. Semantic validation belongs in the configuration or request-handling
layer.

The sample ECU logical address is `0x1001`, in the vehicle-manufacturer-
specific range from Table 39. It was previously `0x0E80`, an address in the
external test-equipment range. M3.2 still needs to validate configured identity
fields, including the Table 40 invalidity values.

Tests assert the VIN/EID request type values directly from Table 12 instead of
only comparing the encoder result with the same constants it uses. The
response-layout test checks exact bytes and uses a manufacturer-specific
logical address.

## Implementation and tests

- `include/doip_uds/vehicle_identification.hpp` declares the payload-type
  constants, request and response structures, and codec interfaces.
- `src/vehicle_identification.cpp` maps request variants to payload types and
  payload bytes, checks request type and exact payload size during decoding,
  and encodes/decodes the fixed-size response representation.
- `tests/unit/test_vehicle_identification.cpp` covers broadcast, VIN-targeted,
  and EID-targeted requests; response byte ordering and round-trip behavior;
  and rejection of unknown types, null pointers, or incorrect sizes.
- The top-level and test `CMakeLists.txt` files include the codec and unit
  tests in the build.

The decoders return `std::optional`: a value indicates that the type/size
checks passed, while `std::nullopt` indicates invalid input. They do not
perform semantic checks on VIN characters, logical addresses, further-action
values, or synchronization-status values.

## Scope and next steps

M3.1 covers payload structures and conversion only. It does not encode or
decode the generic header, load `config/ecu_config.json`, match a request to
this ECU, or send/receive UDP datagrams. When constructing a complete
vehicle-identification request, the generic-header protocol-version byte must
be `kDoipDefaultProtocolVersionForVehicleIdentificationRequest` (`0xFF`) with
inverse `0x00` as defined by Table 11. This request-specific default does not
change the normal M2 header-codec default of `0x02`.

The later M3 steps build on this codec:

- **M3.2:** load and validate the ECU identity from configuration.
- **M3.3:** add the UDP listener and datagram transport.
- **M3.4:** connect incoming requests to ECU identity matching and response
  generation.
- **M3.5:** verify the complete exchange over localhost and document usage.

Before claiming conformance to a later edition, cross-check its requirements
and update the constants, tests, and documentation as needed.
