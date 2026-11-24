#!/usr/bin/env python3
"""Verify that SIGTERM triggers an orderly server shutdown."""

import argparse
import selectors
import signal
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--config", required=True)
    args = parser.parse_args()

    process = subprocess.Popen(
        [
            args.server,
            "--config",
            args.config,
            "--bind-address",
            "127.0.0.1",
            "--tcp-port",
            "0",
            "--udp-port",
            "0",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    output = []
    try:
        deadline = time.monotonic() + 10
        ready = False
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    break
                if not selector.select(timeout=0.1):
                    continue
                line = process.stdout.readline()
                output.append(line)
                if line.startswith("Listening for DoIP"):
                    ready = True
                    break
        if not ready:
            raise RuntimeError("Server did not start listening:\n" + "".join(output))

        process.send_signal(signal.SIGTERM)
        remaining, _ = process.communicate(timeout=5)
        output.append(remaining)
        complete_output = "".join(output)
        if process.returncode != 0:
            raise RuntimeError(
                f"Server exited with status {process.returncode}:\n{complete_output}"
            )
        if "DoIP server stopped." not in complete_output:
            raise RuntimeError("Server did not report graceful shutdown:\n" + complete_output)
        print("SIGTERM produced a clean server shutdown.")
        return 0
    except Exception as error:
        if process.poll() is None:
            process.kill()
            process.wait()
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
