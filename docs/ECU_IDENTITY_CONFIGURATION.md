# ECU identity configuration (M3.2)

M3.2 loads the ECU's vehicle-identification values from
`config/ecu_config.json` and validates them before the server starts. The
resulting `doip_uds::EcuIdentity` contains a fixed-size VIN, a 16-bit logical
address, and fixed-size EID and GID byte arrays.

## Configuration fields

```json
{
  "logical_address": "0x1001",
  "vin": "WVWZZZ00000000001",
  "eid": "00:00:00:00:00:01",
  "gid": "00:00:00:00:00:02",
  "dids": [],
  "dtcs": []
}
```

`logical_address`, `vin`, `eid`, and `gid` are required JSON strings.
Additional fields such as `dids` and `dtcs` are ignored by M3.2 because those
belong to later diagnostic-service milestones.

- **VIN:** exactly 17 uppercase VIN characters (`0-9`, `A-H`, `J-N`, `P`,
  `R-Z`); the all-zero and all-`0xFF` invalidity values are not accepted.
- **Logical address:** exactly four hexadecimal digits with a `0x` prefix.
  Accepted vehicle-manufacturer ranges are `0x0001`-`0x0DFF` and
  `0x1000`-`0x7FFF`; external test-equipment and reserved ranges are rejected.
- **EID and GID:** six hexadecimal byte pairs separated by colons. The all-zero
  and all-`0xFF` invalidity values are rejected.

These identity checks follow the project's supplied ISO 13400-2:2012(E)
reference, in particular Table 39 (logical-address ranges) and Table 40
(invalidity values). The validation is specific to that edition and is not a
claim of complete ISO conformance.

## Loading and errors

The public API is declared in `include/doip_uds/ecu_identity.hpp`:

```cpp
const auto identity = doip_uds::load_ecu_identity("config/ecu_config.json");
```

The loader throws `std::runtime_error` when the file cannot be opened, JSON is
malformed, required fields are missing or have the wrong type, or values fail
validation. The server executable loads `config/ecu_config.json` by default;
an alternate path can be supplied as its single command-line argument. A
failure is reported to standard error and causes a nonzero exit status.

The parser uses `nlohmann/json`; CMake first searches for an installed package
and otherwise fetches the pinned 3.11.3 release, which requires network access
on the first configure when the package is not installed.
