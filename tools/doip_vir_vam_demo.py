#!/usr/bin/env python3
"""Send a DoIP vehicle-identification request and print the ECU's VAM response."""

import argparse
import socket
import struct
import sys


REQUEST = bytes.fromhex("ff 00 00 01 00 00 00 00")
EXPECTED_RESPONSE_HEADER = (0x02, 0xFD, 0x0004, 33)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--destination",
        default="255.255.255.255",
        help="IPv4 destination for the broadcast VIR (default: 255.255.255.255)",
    )
    parser.add_argument("--port", type=int, default=13400, help="DoIP UDP port")
    parser.add_argument("--timeout", type=float, default=3.0, help="response timeout in seconds")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            client.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            client.settimeout(args.timeout)
            client.sendto(REQUEST, (args.destination, args.port))
            response, peer = client.recvfrom(1024)
    except OSError as error:
        print(f"DoIP discovery failed: {error}", file=sys.stderr)
        return 1

    if len(response) != 41:
        print(f"Unexpected response length: {len(response)} bytes (expected 41)", file=sys.stderr)
        return 1

    version, inverse_version, payload_type, payload_length = struct.unpack(">BBHI", response[:8])
    if (version, inverse_version, payload_type, payload_length) != EXPECTED_RESPONSE_HEADER:
        print("Received a datagram that is not a valid vehicle-identification response", file=sys.stderr)
        return 1

    vin = response[8:25].decode("ascii", errors="replace")
    logical_address = int.from_bytes(response[25:27], "big")
    eid = response[27:33].hex(":").upper()
    gid = response[33:39].hex(":").upper()
    print(f"VIR sent to {args.destination}:{args.port}")
    print(f"VAM response from {peer[0]}:{peer[1]}")
    print(f"VIN: {vin}")
    print(f"Logical address: 0x{logical_address:04X}")
    print(f"EID: {eid}")
    print(f"GID: {gid}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
