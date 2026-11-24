#!/usr/bin/env python3
"""Check unicast UDP vehicle-identification against a local DoIP server."""

import argparse
import socket
import struct
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=13400)
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    request = bytes((0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00))
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
            client.settimeout(args.timeout)
            client.sendto(request, (args.host, args.port))
            response, peer = client.recvfrom(1024)
    except OSError as error:
        print(f"UDP vehicle-identification request failed: {error}", file=sys.stderr)
        return 1

    if len(response) != 41:
        print(
            f"Expected a 41-byte VAM response, received {len(response)} bytes",
            file=sys.stderr,
        )
        return 1

    version, inverse_version, payload_type, payload_length = struct.unpack(">BBHI", response[:8])
    if (version, inverse_version, payload_type, payload_length) != (0x02, 0xFD, 0x0004, 33):
        print("Received an invalid DoIP vehicle-identification response", file=sys.stderr)
        return 1

    try:
        vin = response[8:25].decode("ascii")
    except UnicodeDecodeError:
        print("Vehicle-identification response contained a non-ASCII VIN", file=sys.stderr)
        return 1
    print(f"UDP vehicle-identification succeeded: VIN {vin} from {peer[0]}:{peer[1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
