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
import signal
import subprocess
import sys
import tempfile
import time
from typing import Callable

CURSOR_POSITION_QUERY = b"\x1b[6n"
CURSOR_POSITION_RESPONSE = b"\x1b[1;1R"

INTERACTIVE_ARGS = [
    "--no-source",
    "--no-titleline",
    "--minimal",
    "--no-prompt-vars",
]


def set_nonblocking(fd: int) -> None:
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)


class InteractiveSession:
    def __init__(self, binary: str, label: str) -> None:
        self.binary = binary
        self.label = label
        self.master_fd = -1
        self.slave_fd = -1
        self.process: subprocess.Popen[bytes] | None = None
        self.output = bytearray()
        self._query_tail = b""
        self._tmp_home: tempfile.TemporaryDirectory[str] | None = None

    def __enter__(self) -> "InteractiveSession":
        try:
            self.master_fd, self.slave_fd = os.openpty()
            self._tmp_home = tempfile.TemporaryDirectory(prefix="cjsh-shutdown-")

            env = os.environ.copy()
            env.setdefault("TERM", "xterm-256color")
            env["HOME"] = self._tmp_home.name
            env["XDG_CONFIG_HOME"] = os.path.join(self._tmp_home.name, ".config")

            self.process = subprocess.Popen(
                [self.binary, *INTERACTIVE_ARGS],
                stdin=self.slave_fd,
                stdout=self.slave_fd,
                stderr=self.slave_fd,
                env=env,
                close_fds=True,
            )
            set_nonblocking(self.master_fd)
            return self
        except Exception:
            self.__exit__(None, None, None)
            raise
        finally:
            if self.slave_fd >= 0:
                os.close(self.slave_fd)
                self.slave_fd = -1

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        if self.process is not None and self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=2.0)

        if self.master_fd >= 0:
            try:
                os.close(self.master_fd)
            except OSError:
                pass
            self.master_fd = -1

        if self.slave_fd >= 0:
            try:
                os.close(self.slave_fd)
            except OSError:
                pass
            self.slave_fd = -1

        if self._tmp_home is not None:
            self._tmp_home.cleanup()
            self._tmp_home = None

    def pump(self, duration_s: float) -> None:
        deadline = time.monotonic() + duration_s
        while time.monotonic() < deadline:
            if self.master_fd < 0:
                return

            try:
                chunk = os.read(self.master_fd, 4096)
            except BlockingIOError:
                chunk = b""
            except OSError as exc:
                if exc.errno == errno.EIO:
                    return
                raise

            if not chunk:
                time.sleep(0.01)
                continue

            self.output.extend(chunk)
            pending = self._query_tail + chunk
            request_count = pending.count(CURSOR_POSITION_QUERY)
            for _ in range(request_count):
                os.write(self.master_fd, CURSOR_POSITION_RESPONSE)
            self._query_tail = pending[-(len(CURSOR_POSITION_QUERY) - 1) :]

    def write(self, data: bytes) -> None:
        if self.master_fd < 0:
            raise AssertionError(f"{self.label}: PTY master is closed")
        os.write(self.master_fd, data)

    def close_master(self) -> None:
        if self.master_fd >= 0:
            os.close(self.master_fd)
            self.master_fd = -1

    def wait_for_exit(self, timeout_s: float) -> int:
        if self.process is None:
            raise AssertionError(f"{self.label}: failed to start cjsh process")

        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            self.pump(0.05)
            return_code = self.process.poll()
            if return_code is not None:
                self.pump(0.1)
                return return_code

        raise AssertionError(f"{self.label}: cjsh did not exit within {timeout_s:.1f}s")

    def assert_running(self) -> None:
        if self.process is None:
            raise AssertionError(f"{self.label}: failed to start cjsh process")
        self.pump(0.1)
        if self.process.poll() is not None:
            raise AssertionError(
                f"{self.label}: cjsh exited unexpectedly with {self.process.returncode}"
            )


def run_hangup_iteration(binary: str, iteration: int) -> None:
    with InteractiveSession(binary, f"hangup iteration {iteration}") as session:
        session.pump(duration_s=0.6)
        session.close_master()

        if session.process is None:
            raise AssertionError("failed to start cjsh process")

        try:
            session.process.wait(timeout=3.0)
        except subprocess.TimeoutExpired as exc:
            raise AssertionError(
                f"iteration {iteration}: cjsh did not exit after PTY hangup"
            ) from exc


def run_ctrl_d_exits(binary: str) -> None:
    with InteractiveSession(binary, "ctrl-d eof") as session:
        session.pump(duration_s=0.6)
        session.write(b"\x04")

        return_code = session.wait_for_exit(timeout_s=3.0)
        if return_code != 0:
            raise AssertionError(f"Ctrl-D exit returned {return_code}, expected 0")


def run_exit_status_command(binary: str) -> None:
    with InteractiveSession(binary, "exit status") as session:
        session.pump(duration_s=0.6)
        session.write(b"exit 37\r")

        return_code = session.wait_for_exit(timeout_s=3.0)
        if return_code != 37:
            raise AssertionError(f"interactive exit returned {return_code}, expected 37")


def run_terminal_region_marking(binary: str) -> None:
    with InteractiveSession(binary, "terminal region marking") as session:
        session.pump(duration_s=0.6)
        session.write(b"false\r")
        session.pump(duration_s=0.8)
        session.assert_running()

        prompt_start = b"\x1b]133;A\x1b\\"
        input_start = b"\x1b]133;B\x1b\\"
        output_start = b"\x1b]133;C\x1b\\"
        output_end = b"\x1b]133;D;1\x1b\\"

        command_start_index = session.output.find(output_start)
        command_end_index = session.output.find(output_end, command_start_index + 1)
        input_start_index = session.output.rfind(input_start, 0, command_start_index)
        prompt_start_index = session.output.rfind(prompt_start, 0, input_start_index)

        if min(
            prompt_start_index,
            input_start_index,
            command_start_index,
            command_end_index,
        ) < 0:
            raise AssertionError(
                f"missing OSC 133 marker in interactive output: {bytes(session.output)!r}"
            )
        if not (
            prompt_start_index
            < input_start_index
            < command_start_index
            < command_end_index
        ):
            raise AssertionError(
                f"OSC 133 markers are out of order: {bytes(session.output)!r}"
            )

        session.write(b"exit 0\r")
        return_code = session.wait_for_exit(timeout_s=3.0)
        if return_code != 0:
            raise AssertionError(
                f"exit after terminal-region probe returned {return_code}, expected 0"
            )


def run_interrupt_then_exit(binary: str) -> None:
    with InteractiveSession(binary, "ctrl-c interrupt") as session:
        session.pump(duration_s=0.6)
        session.write(b"\x03")
        session.assert_running()
        session.write(b"exit 0\r")

        return_code = session.wait_for_exit(timeout_s=3.0)
        if return_code != 0:
            raise AssertionError(f"exit after Ctrl-C returned {return_code}, expected 0")


def run_interrupt_with_queued_exit(binary: str) -> None:
    with InteractiveSession(binary, "ctrl-c queued exit") as session:
        session.pump(duration_s=0.6)
        session.write(b"\x03exit 0\r")

        return_code = session.wait_for_exit(timeout_s=3.0)
        if return_code != 0:
            raise AssertionError(f"queued exit after Ctrl-C returned {return_code}, expected 0")


def run_sigterm_at_prompt(binary: str) -> None:
    with InteractiveSession(binary, "sigterm at prompt") as session:
        session.pump(duration_s=0.6)
        session.assert_running()

        if session.process is None:
            raise AssertionError("failed to start cjsh process")

        os.kill(session.process.pid, signal.SIGTERM)
        return_code = session.wait_for_exit(timeout_s=3.0)
        expected_codes = {128 + signal.SIGTERM, -signal.SIGTERM}
        if return_code not in expected_codes:
            raise AssertionError(
                f"SIGTERM exit returned {return_code}, expected one of {sorted(expected_codes)}"
            )


def run_cases(cases: list[tuple[str, Callable[[], None]]], suite_name: str) -> int:
    failures = 0
    for label, case in cases:
        try:
            case()
        except Exception as exc:
            failures += 1
            message = f"{type(exc).__name__}: {exc}".replace("\n", "\n    ")
            print(f"FAIL: {label}: {message}", file=sys.stderr)
        else:
            print(f"PASS: {label}")

    if failures:
        print(f"{failures}/{len(cases)} {suite_name} tests failed", file=sys.stderr)
        return 1

    print(f"All {len(cases)} {suite_name} tests passed")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print("usage: test_interactive_shutdown.py <cjsh_binary>", file=sys.stderr)
        return 2

    binary = argv[0]
    cases: list[tuple[str, Callable[[], None]]] = [
        (
            f"hangup iteration {iteration}",
            lambda iteration=iteration: run_hangup_iteration(binary, iteration),
        )
        for iteration in range(1, 6)
    ]
    cases.extend(
        [
            ("ctrl-d exits", lambda: run_ctrl_d_exits(binary)),
            ("exit status command", lambda: run_exit_status_command(binary)),
            ("terminal region marking", lambda: run_terminal_region_marking(binary)),
            ("ctrl-c then exit", lambda: run_interrupt_then_exit(binary)),
            ("ctrl-c queued exit", lambda: run_interrupt_with_queued_exit(binary)),
            ("sigterm at prompt", lambda: run_sigterm_at_prompt(binary)),
        ]
    )

    return run_cases(cases, "interactive shutdown")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
