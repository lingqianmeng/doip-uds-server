# DoIP generic-header codec (M2)

M2 adds a codec for converting the DoIP generic header between an 8-byte wire
representation and a C++ structure. It does not implement networking or UDS
services.

## DoIP message structure

Every DoIP message starts with an 8-byte generic header, followed by its
payload:

```text
+----------------------+---------------------+
| 8-byte generic header| payload (0+ bytes)  |
+----------------------+---------------------+
```

The generic header identifies the payload type and its length:

| Bytes | Field | Meaning |
|---|---|---|
| 0 | Protocol version | Identifies the DoIP protocol version |
| 1 | Inverse version | Bitwise complement of the version byte; a consistency check |
| 2-3 | Payload type | Identifies the kind of message |
| 4-7 | Payload length | Number of payload bytes following the header |

Multi-byte values use **network byte order** (big-endian): the most significant
byte comes first.

For vehicle-identification request messages, ISO 13400-2:2012(E) Table 11
defines `0xFF` as the default protocol-version byte, with inverse `0x00`.
This request-specific default allows discovery before the sender knows which
DoIP protocol version a responding entity uses. The normal M2 default in
`DoipHeader` remains `0x02`; code constructing an identification request must
select the dedicated request-default value explicitly.

For example, the encoder test produces this header:

```text
02 FD 80 01 12 34 56 78
```

Interpreted field by field:

- `02`: protocol version `0x02`
- `FD`: bitwise inverse of `0x02` in 8 bits
- `80 01`: payload type `0x8001` (diagnostic message)
- `12 34 56 78`: payload length `0x12345678`

The length is deliberately large to make byte order easy to see in the test; it
is not a realistic message size.

## Implementation

- `include/doip_uds/doip_header.hpp` declares `DoipHeader`, the 8-byte header
  size constant, and the encode/decode interfaces.
- `src/doip_header.cpp` implements conversion between the structure's integer
  fields and the header bytes.
- `tests/unit/test_doip_header.cpp` covers encoding, decoding, truncated input,
  a null input pointer, and an invalid version-inverse pair.
- The top-level and test `CMakeLists.txt` files add the implementation and tests
  to their respective targets.

`decode_doip_header()` returns `std::optional<DoipHeader>`. It contains a
decoded header on success, or `std::nullopt` if the input pointer is null, the
buffer is shorter than 8 bytes, or the inverse-version byte does not match.

## Scope and limitations

This codec currently:

- reads and writes the 8-byte generic header;
- converts multi-byte fields using big-endian byte order;
- checks that the inverse-version byte matches the protocol-version byte.

It does **not** currently:

- reject unsupported protocol versions;
- validate payload types or payload lengths against DoIP rules;
- verify that the complete declared payload has arrived;
- read from or write to sockets.

For example, the decoder accepts version `0x03` when its inverse byte is
correct. It validates the version/inverse relationship, not which versions the
server supports.

The payload length will be important when transport is added. In particular,
TCP is a byte stream: one socket read is not guaranteed to contain exactly one
complete DoIP message. The receiver will need to use the header's declared
length to assemble and parse complete messages.

## How this fits the roadmap

The codec provides the first step for interpreting a DoIP message. Later work
can use the decoded payload type to select message handling and the length to
read the corresponding payload. M3 adds UDP vehicle identification; TCP
transport and diagnostic-message handling are planned for M4.
