#!/usr/bin/env python3
"""Run the simulator's DoIP/UDS smoke test against localhost."""

import argparse
import socket
import struct
import sys


PROTOCOL_VERSION = 0x02
INVERSE_PROTOCOL_VERSION = 0xFD
MAX_PAYLOAD_SIZE = 16 * 1024 * 1024
ROUTING_ACTIVATION_REQUEST = 0x0005
ROUTING_ACTIVATION_RESPONSE = 0x0006
DIAGNOSTIC_MESSAGE = 0x8001
DIAGNOSTIC_MESSAGE_ACK = 0x8002
DIAGNOSTIC_MESSAGE_NACK = 0x8003
SECURITY_KEY_MASK = 0x0000A5C3


class DoipError(Exception):
    """Raised when the simulator response violates the expected DoIP flow."""


class DoipClient:
    def __init__(self, port, tester_address, ecu_address, timeout):
        self.port = port
        self.tester_address = tester_address
        self.ecu_address = ecu_address
        self.timeout = timeout
        self.connection = None

    def __enter__(self):
        try:
            self.connection = socket.create_connection(
                ("127.0.0.1", self.port), timeout=self.timeout
            )
            self.connection.settimeout(self.timeout)
        except OSError as error:
            raise DoipError(
                f"Could not connect to the local DoIP server on TCP port {self.port}: {error}"
            ) from error
        return self

    def __exit__(self, _exception_type, _exception, _traceback):
        if self.connection is not None:
            self.connection.close()
            self.connection = None

    def _send_frame(self, payload_type, payload):
        if self.connection is None:
            raise DoipError("The DoIP connection is not open")
        if len(payload) > MAX_PAYLOAD_SIZE:
            raise DoipError("DoIP payload exceeds the 16 MiB client limit")
        header = struct.pack(
            ">BBHI", PROTOCOL_VERSION, INVERSE_PROTOCOL_VERSION, payload_type, len(payload)
        )
        try:
            self.connection.sendall(header + payload)
        except OSError as error:
            raise DoipError(f"Failed to send DoIP frame: {error}") from error

    def _receive_exactly(self, length):
        if self.connection is None:
            raise DoipError("The DoIP connection is not open")
        data = bytearray()
        try:
            while len(data) < length:
                chunk = self.connection.recv(length - len(data))
                if not chunk:
                    raise DoipError("The server closed the connection during a DoIP frame")
                data.extend(chunk)
        except socket.timeout as error:
            raise DoipError("Timed out while waiting for a DoIP frame") from error
        except OSError as error:
            raise DoipError(f"Failed to receive DoIP frame: {error}") from error
        return bytes(data)

    def _receive_frame(self):
        header = self._receive_exactly(8)
        version, inverse_version, payload_type, payload_length = struct.unpack(">BBHI", header)
        if (version, inverse_version) != (PROTOCOL_VERSION, INVERSE_PROTOCOL_VERSION):
            raise DoipError(
                f"Invalid DoIP version preamble: 0x{version:02X} 0x{inverse_version:02X}"
            )
        if payload_length > MAX_PAYLOAD_SIZE:
            raise DoipError(f"DoIP payload length {payload_length} exceeds the client limit")
        return payload_type, self._receive_exactly(payload_length)

    def activate_routing(self):
        request = struct.pack(">HB", self.tester_address, 0x00) + bytes(4)
        self._send_frame(ROUTING_ACTIVATION_REQUEST, request)
        payload_type, payload = self._receive_frame()
        if payload_type != ROUTING_ACTIVATION_RESPONSE or len(payload) != 9:
            raise DoipError("Received an invalid Routing Activation Response")
        tester_address, ecu_address, response_code = struct.unpack(">HHB", payload[:5])
        if (
            tester_address != self.tester_address
            or ecu_address != self.ecu_address
            or response_code != 0x10
        ):
            raise DoipError(
                "Routing activation was rejected or returned unexpected logical addresses"
            )

    def diagnostic(self, uds_request):
        if not uds_request:
            raise DoipError("A UDS request must contain a service identifier")
        payload = struct.pack(">HH", self.tester_address, self.ecu_address) + uds_request
        self._send_frame(DIAGNOSTIC_MESSAGE, payload)

        payload_type, acknowledgement = self._receive_frame()
        if payload_type == DIAGNOSTIC_MESSAGE_NACK:
            if len(acknowledgement) == 5:
                raise DoipError(
                    f"DoIP Diagnostic Message NACK: code 0x{acknowledgement[4]:02X}"
                )
            raise DoipError("Received a malformed DoIP Diagnostic Message NACK")
        if payload_type != DIAGNOSTIC_MESSAGE_ACK or len(acknowledgement) != 5:
            raise DoipError("Received an invalid DoIP Diagnostic Message ACK")
        ack_source, ack_target, ack_code = struct.unpack(">HHB", acknowledgement)
        if (
            ack_source != self.ecu_address
            or ack_target != self.tester_address
            or ack_code != 0
        ):
            raise DoipError("Diagnostic Message ACK had unexpected addresses or code")

        payload_type, response = self._receive_frame()
        if payload_type == DIAGNOSTIC_MESSAGE_NACK:
            if len(response) == 5:
                raise DoipError(f"DoIP Diagnostic Message NACK: code 0x{response[4]:02X}")
            raise DoipError("Received a malformed DoIP Diagnostic Message NACK")
        if payload_type != DIAGNOSTIC_MESSAGE or len(response) < 5:
            raise DoipError("Received an invalid UDS Diagnostic Message response")
        response_source, response_target = struct.unpack(">HH", response[:4])
        if response_source != self.ecu_address or response_target != self.tester_address:
            raise DoipError("UDS response had unexpected logical addresses")
        return response[4:]


def expect_response(client, request, expected, label):
    response = client.diagnostic(request)
    if response != expected:
        raise DoipError(
            f"{label}: expected {expected.hex(' ').upper()}, "
            f"received {response.hex(' ').upper()}"
        )
    print(f"{label}: {response.hex(' ').upper()}")
    return response


def run_demo(port, tester_address, ecu_address, timeout):
    with DoipClient(port, tester_address, ecu_address, timeout) as client:
        client.activate_routing()
        print(
            f"Routing activation succeeded: tester 0x{tester_address:04X}, "
            f"ECU 0x{ecu_address:04X}"
        )

        vin_response = client.diagnostic(bytes.fromhex("22 F1 90"))
        if len(vin_response) != 20 or vin_response[:3] != bytes.fromhex("62 F1 90"):
            raise DoipError("Read VIN returned an unexpected response")
        print(f"VIN: {vin_response[3:].decode('ascii')}")

        initial_did = client.diagnostic(bytes.fromhex("22 F1 A0"))
        if len(initial_did) != 7 or initial_did[:3] != bytes.fromhex("62 F1 A0"):
            raise DoipError("Read writable DID returned an unexpected response")
        print(f"Initial DID F1A0: {initial_did[3:].hex(' ').upper()}")

        expect_response(
            client,
            bytes.fromhex("2E F1 A0 11 22 33 44"),
            bytes.fromhex("7F 2E 7E"),
            "Reject write in default session",
        )
        expect_response(
            client,
            bytes.fromhex("10 03"),
            bytes.fromhex("50 03 00 32 01 F4"),
            "Enter extended session",
        )
        expect_response(
            client,
            bytes.fromhex("2E F1 A0 11 22 33 44"),
            bytes.fromhex("7F 2E 33"),
            "Reject write before SecurityAccess",
        )

        seed_response = client.diagnostic(bytes.fromhex("27 5F"))
        if len(seed_response) != 6 or seed_response[:2] != bytes.fromhex("67 5F"):
            raise DoipError("Request AirbagDeployment seed returned an unexpected response")
        seed = int.from_bytes(seed_response[2:], byteorder="big")
        key = seed ^ SECURITY_KEY_MASK
        key_request = bytes([0x27, 0x60]) + key.to_bytes(4, byteorder="big")
        print(f"Request AirbagDeployment seed: {seed_response.hex(' ').upper()}")
        expect_response(
            client,
            key_request,
            bytes.fromhex("67 60"),
            f"Send simulator key 0x{key:08X}",
        )
        expect_response(
            client,
            bytes.fromhex("2E F1 A0 11 22 33 44"),
            bytes.fromhex("6E F1 A0"),
            "Write DID F1A0",
        )
        updated_did = client.diagnostic(bytes.fromhex("22 F1 A0"))
        expected_did = bytes.fromhex("62 F1 A0 11 22 33 44")
        if updated_did != expected_did:
            raise DoipError(
                f"DID readback mismatch: expected {expected_did.hex(' ').upper()}, "
                f"received {updated_did.hex(' ').upper()}"
            )
        print(f"DID F1A0 readback: {updated_did[3:].hex(' ').upper()}")

        vin_write = client.diagnostic(bytes.fromhex("2E F1 90 00"))
        if vin_write != bytes.fromhex("7F 2E 31"):
            raise DoipError(
                f"Write to read-only VIN returned {vin_write.hex(' ').upper()}"
            )
        print(f"Reject write to read-only VIN: {vin_write.hex(' ').upper()}")

        dtc_response = client.diagnostic(bytes.fromhex("19 01 09"))
        if (
            len(dtc_response) != 6
            or dtc_response[:4] != bytes.fromhex("59 01 FF 01")
        ):
            raise DoipError("Read DTC count returned an unexpected response")
        dtc_count = int.from_bytes(dtc_response[4:], byteorder="big")
        print(f"Configured DTC count: {dtc_count}")


def parse_address(value):
    try:
        address = int(value, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be an integer such as 0x0E00") from error
    if not 0 <= address <= 0xFFFF:
        raise argparse.ArgumentTypeError("must be between 0 and 65535")
    return address


def parse_port(value):
    try:
        port = int(value, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be an integer") from error
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("must be between 1 and 65535")
    return port


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    demo_parser = commands.add_parser("demo", help="run the local simulator smoke test")
    demo_parser.add_argument("--port", type=parse_port, default=13400, help="local DoIP TCP port")
    demo_parser.add_argument(
        "--tester-address",
        type=parse_address,
        default=0x0E00,
        help="tester logical address (default: 0x0E00)",
    )
    demo_parser.add_argument(
        "--ecu-address",
        type=parse_address,
        default=0x1001,
        help="simulator logical address (default: 0x1001)",
    )
    demo_parser.add_argument(
        "--timeout", type=float, default=3.0, help="socket timeout in seconds"
    )
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    try:
        run_demo(args.port, args.tester_address, args.ecu_address, args.timeout)
    except DoipError as error:
        print(f"DoIP tester failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
