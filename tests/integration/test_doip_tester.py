#!/usr/bin/env python3
"""Exercise the local Python tester against the built DoIP server."""

import argparse
import importlib.util
import queue
import re
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TESTER_SCRIPT = REPOSITORY_ROOT / "tools" / "doip_tester.py"
TESTER_MODULE_SPEC = importlib.util.spec_from_file_location(
    "doip_tester", TESTER_SCRIPT
)
doip_tester = importlib.util.module_from_spec(TESTER_MODULE_SPEC)
TESTER_MODULE_SPEC.loader.exec_module(doip_tester)


class DoipTesterIntegration(unittest.TestCase):
    server_path = None
    config_path = None
    server_process = None
    server_output = []
    output_queue = None
    output_reader = None
    tcp_port = None

    @classmethod
    def setUpClass(cls):
        command = [
            str(cls.server_path),
            "--config",
            str(cls.config_path),
            "--bind-address",
            "127.0.0.1",
            "--tcp-port",
            "0",
            "--udp-port",
            "0",
        ]
        cls.server_process = subprocess.Popen(
            command,
            cwd=REPOSITORY_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        cls.output_queue = queue.Queue()
        cls.output_reader = threading.Thread(
            target=cls._read_server_output, daemon=True
        )
        cls.output_reader.start()

        deadline = time.monotonic() + 10.0
        listening_pattern = re.compile(
            r"Listening for DoIP vehicle-identification requests on UDP port "
            r"(\d+) and TCP connections on port (\d+)"
        )
        try:
            while time.monotonic() < deadline:
                if cls.server_process.poll() is not None:
                    raise RuntimeError("DoIP server exited during startup")
                try:
                    line = cls.output_queue.get(timeout=max(0.01, deadline - time.monotonic()))
                except queue.Empty as error:
                    raise RuntimeError("Timed out waiting for the DoIP server to start") from error
                if line is None:
                    raise RuntimeError("DoIP server closed its output during startup")
                cls.server_output.append(line)
                match = listening_pattern.search(line)
                if match:
                    cls.tcp_port = int(match.group(2))
                    return
            raise RuntimeError("Timed out waiting for the DoIP server to start")
        except Exception:
            cls._stop_server()
            raise

    @classmethod
    def tearDownClass(cls):
        cls._stop_server()

    @classmethod
    def _read_server_output(cls):
        if cls.server_process is None or cls.server_process.stdout is None:
            return
        for line in cls.server_process.stdout:
            cls.output_queue.put(line)
        cls.output_queue.put(None)

    @classmethod
    def _stop_server(cls):
        if cls.server_process is None:
            return
        if cls.server_process.poll() is None:
            cls.server_process.terminate()
            try:
                cls.server_process.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                cls.server_process.kill()
                cls.server_process.wait(timeout=5.0)
        if cls.output_reader is not None:
            cls.output_reader.join(timeout=1.0)

    def test_local_demo_completes_and_verifies_did_write(self):
        result = subprocess.run(
            [
                sys.executable,
                str(TESTER_SCRIPT),
                "demo",
                "--port",
                str(self.tcp_port),
            ],
            cwd=REPOSITORY_ROOT,
            capture_output=True,
            text=True,
            timeout=15.0,
            check=False,
        )
        self.assertEqual(
            result.returncode,
            0,
            msg=f"Tester failed:\n{result.stdout}\n{result.stderr}",
        )
        self.assertIn("DID F1A0 readback: 11 22 33 44", result.stdout)
        self.assertIn("Configured DTC count: 10", result.stdout)

    def test_bootloader_security_access_requires_programming_session(self):
        with doip_tester.DoipClient(
            self.tcp_port, tester_address=0x0E00, ecu_address=0x1001, timeout=3.0
        ) as client:
            client.activate_routing()
            self.assertEqual(
                client.diagnostic(bytes.fromhex("27 11")),
                bytes.fromhex("7F 27 22"),
            )
            self.assertEqual(
                client.diagnostic(bytes.fromhex("10 02")),
                bytes.fromhex("50 02 00 32 01 F4"),
            )
            seed_response = client.diagnostic(bytes.fromhex("27 11"))
            self.assertEqual(seed_response[:2], bytes.fromhex("67 11"))
            self.assertEqual(len(seed_response), 6)
            seed = int.from_bytes(seed_response[2:], byteorder="big")
            key = seed ^ doip_tester.SECURITY_KEY_MASK
            key_request = bytes([0x27, 0x12]) + key.to_bytes(4, byteorder="big")
            self.assertEqual(
                client.diagnostic(key_request),
                bytes.fromhex("67 12"),
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", required=True, help="path to the built doip_server executable")
    parser.add_argument("--config", required=True, help="path to the ECU configuration JSON")
    args = parser.parse_args()

    DoipTesterIntegration.server_path = Path(args.server).resolve()
    DoipTesterIntegration.config_path = Path(args.config).resolve()
    if not DoipTesterIntegration.server_path.is_file():
        parser.error(f"server executable not found: {DoipTesterIntegration.server_path}")
    if not DoipTesterIntegration.config_path.is_file():
        parser.error(f"ECU configuration not found: {DoipTesterIntegration.config_path}")
    unittest.main(argv=[sys.argv[0]])


if __name__ == "__main__":
    main()
