#!/usr/bin/env python3

# test_child_signal_startup.py
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from __future__ import annotations

import concurrent.futures
import os
import signal
import subprocess
import sys


def check_child(binary: str, command: str) -> str | None:
    process = subprocess.Popen(
        [binary, "--secure", "-c", command],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        stdout, stderr = process.communicate()
        return f"child did not terminate: {stdout!r} {stderr!r}"
    if process.returncode != 128 + signal.SIGTERM:
        return f"exit={process.returncode}, stdout={stdout!r}, stderr={stderr!r}"
    return None


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <cjsh-binary>", file=sys.stderr)
        return 2
    binary = os.path.abspath(sys.argv[1])
    failures = []
    # Competing launches expose signals delivered while a child is still inside
    # fork. Keep the kill immediate and verify the actual signal exit status.
    for command in ("sleep 2", "true | sleep 2"):
        script = command + ' & pid=$!; kill -TERM "$pid" || exit 1; wait "$pid"'
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = pool.map(lambda _: check_child(binary, script), range(512))
            for attempt, error in enumerate(results, 1):
                if error is not None:
                    failures.append(f"{command}, attempt {attempt}: {error}")
    if failures:
        print(f"FAIL: {len(failures)}/1024 child launches lost SIGTERM")
        for failure in failures[:10]:
            print(failure)
        return 1
    print("PASS: all 1024 child launches preserved SIGTERM status")
    return 0


if __name__ == "__main__":
    sys.exit(main())
