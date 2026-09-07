#!/usr/bin/env python3

# test_terminal_selection.py
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

"""A redirected PTY must not give cjsh ownership of its caller's terminal."""

from __future__ import annotations

import errno
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import termios
import unittest

TESTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TESTS))
sys.path.insert(0, str(TESTS / "runner"))
from time_startup_binaries import run_startup_test
from test_terminal_safety import run_in_terminal


def run_probe(binary: str, scenario: str, directory: str) -> None:
    os.environ.update(HOME=directory, XDG_CONFIG_HOME=directory,
                      CJSH_CONFIG_HOME=directory, TERM="xterm-256color")
    # Fail if the environment blocks /dev/tty: otherwise the broken selection
    # falls back to stdin and this regression would incorrectly pass.
    controlling = os.open("/dev/tty", os.O_RDWR)
    try:
        original_group = os.tcgetpgrp(controlling)
        original_modes = termios.tcgetattr(controlling)
        assert original_group == os.getpgrp()
        for _ in range(2):
            if scenario == "startup":
                assert run_startup_test(Path(binary)) >= 0
            else:
                run_redirected_command(binary, scenario)
            assert os.tcgetpgrp(controlling) == original_group, "caller lost foreground ownership"
            assert termios.tcgetattr(controlling) == original_modes, "caller terminal modes changed"
    finally:
        os.close(controlling)
    print("terminal-selection-ok", flush=True)


def run_redirected_command(binary: str, scenario: str) -> None:
    master, slave = os.openpty()
    readonly = -1
    process = None
    try:
        assert os.fstat(slave).st_rdev != os.fstat(0).st_rdev
        if scenario == "readonly-stdin":
            readonly = os.open(os.ttyname(slave), os.O_RDONLY | os.O_NOCTTY)
        process = subprocess.Popen(
            [binary, "--no-config", "--minimal", "--no-history", "-i", "-c",
             "printf 'child-ok\\n'"],
            stdin=(subprocess.DEVNULL if scenario == "stdout" else
                   readonly if readonly >= 0 else slave),
            stdout=slave if scenario == "stdout" else subprocess.PIPE,
            stderr=subprocess.PIPE,
            close_fds=True,
        )
        # Keep the slave open while draining stdout: macOS can discard unread
        # PTY output when the final slave descriptor closes.
        stdout, stderr = process.communicate(timeout=5)
        assert process.returncode == 0, (process.returncode, stderr)
        if scenario == "stdout":
            os.set_blocking(master, False)
            output = bytearray()
            while True:
                try:
                    chunk = os.read(master, 4096)
                except OSError as exc:
                    if exc.errno in (errno.EIO, errno.EAGAIN):
                        break
                    raise
                if not chunk:
                    break
                output.extend(chunk)
            stdout = bytes(output)
        assert b"child-ok" in stdout, (stdout, stderr)
    finally:
        if process is not None:
            if process.poll() is None:
                process.kill()
            process.wait()
        for fd in (master, slave, readonly):
            if fd >= 0:
                os.close(fd)


class TerminalSelectionTests(unittest.TestCase):
    binary: str

    def check_selection(self, scenario: str) -> None:
        with tempfile.TemporaryDirectory(prefix="cjsh-terminal-selection-") as directory:
            code, output = run_in_terminal(
                [sys.executable, str(Path(__file__).resolve()), "--probe",
                 self.binary, scenario, directory], timeout=15,
            )
        self.assertEqual(code, 0, output)
        self.assertIn("terminal-selection-ok", output)

    def test_repeated_startup_benchmarks_preserve_callers_terminal(self) -> None:
        self.check_selection("startup")

    def test_interactive_command_prefers_stdin_pty(self) -> None:
        self.check_selection("stdin")

    def test_interactive_command_prefers_stdout_pty_with_redirected_stdin(self) -> None:
        self.check_selection("stdout")

    def test_interactive_command_accepts_readonly_stdin_pty(self) -> None:
        self.check_selection("readonly-stdin")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--probe":
        run_probe(*sys.argv[2:])
    else:
        TerminalSelectionTests.binary = str(Path(sys.argv.pop(1)).resolve())
        unittest.main(verbosity=2)
