#!/usr/bin/env python3

# test_interactive_shutdown.py
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

import errno
import fcntl
import os
import subprocess
import sys
import tempfile
import time


def set_nonblocking(fd: int) -> None:
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)


def service_terminal_queries(fd: int, duration_s: float) -> None:
    deadline = time.monotonic() + duration_s
    pending = b""
    while time.monotonic() < deadline:
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            chunk = b""
        except OSError as exc:
            if exc.errno == errno.EIO:
                return
            raise

        if not chunk:
            time.sleep(0.01)
            continue

        pending += chunk
        request_count = pending.count(b"\x1b[6n")
        for _ in range(request_count):
            os.write(fd, b"\x1b[1;1R")
        if len(pending) > 8:
            pending = pending[-8:]


def run_hangup_iteration(binary: str, iteration: int) -> None:
    master_fd, slave_fd = os.openpty()
    env = os.environ.copy()
    env.setdefault("TERM", "xterm-256color")

    process: subprocess.Popen[bytes] | None = None
    try:
        with tempfile.TemporaryDirectory(prefix="cjsh-shutdown-") as tmp_home:
            env["HOME"] = tmp_home
            env["XDG_CONFIG_HOME"] = os.path.join(tmp_home, ".config")

            process = subprocess.Popen(
                [binary, "--no-source", "--no-titleline", "--minimal"],
                stdin=slave_fd,
                stdout=slave_fd,
                stderr=slave_fd,
                env=env,
                close_fds=True,
            )

            os.close(slave_fd)
            slave_fd = -1
            set_nonblocking(master_fd)

            service_terminal_queries(master_fd, duration_s=0.6)
            os.close(master_fd)
            master_fd = -1

            if process is None:
                raise AssertionError("failed to start cjsh process")

            try:
                process.wait(timeout=3.0)
            except subprocess.TimeoutExpired as exc:
                raise AssertionError(
                    f"iteration {iteration}: cjsh did not exit after PTY hangup"
                ) from exc
    finally:
        if slave_fd >= 0:
            try:
                os.close(slave_fd)
            except OSError:
                pass

        if master_fd >= 0:
            try:
                os.close(master_fd)
            except OSError:
                pass

        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=2.0)


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print("usage: test_interactive_shutdown.py <cjsh_binary>", file=sys.stderr)
        return 2

    binary = argv[0]
    for i in range(5):
        run_hangup_iteration(binary, i + 1)

    print("interactive shutdown hangup test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
