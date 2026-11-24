#!/usr/bin/env python3
"""Verify that SIGTERM triggers an orderly server shutdown."""

import argparse
import queue
import signal
import subprocess
import sys
import threading
import time


def read_output(stream, lines):
    for line in stream:
        lines.put(line)
    lines.put(None)


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
    output_queue = queue.Queue()
    reader = threading.Thread(
        target=read_output, args=(process.stdout, output_queue), daemon=True
    )
    reader.start()
    try:
        deadline = time.monotonic() + 10
        ready = False
        while time.monotonic() < deadline:
            if process.poll() is not None:
                break
            try:
                line = output_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            if line is None:
                break
            output.append(line)
            if line.startswith("Listening for DoIP"):
                ready = True
                break
        if not ready:
            raise RuntimeError("Server did not start listening:\n" + "".join(output))

        process.send_signal(signal.SIGTERM)
        process.wait(timeout=5)
        reader.join(timeout=1)
        while True:
            try:
                line = output_queue.get_nowait()
            except queue.Empty:
                break
            if line is not None:
                output.append(line)
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
        reader.join(timeout=1)
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
