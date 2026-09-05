#!/usr/bin/env python3

# test_idle_hook_interactive.py
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
import pty
import re
import shlex
import signal
import sys
import tempfile
import time


CURSOR_QUERY = b"\x1b[6n"
CURSOR_RESPONSE = b"\x1b[1;1R"
PROMPT_INPUT_START = b"\x1b]133;B\x1b\\"
COMMAND_OUTPUT_END = b"\x1b]133;D;"
ANSI_CSI_RE = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
ANSI_OSC_RE = re.compile(rb"\x1b\].*?(?:\x07|\x1b\\)", re.S)


def normalize_terminal_output(output: bytes) -> bytes:
    normalized = output.replace(b"\r", b"")
    normalized = ANSI_OSC_RE.sub(b"", normalized)
    return ANSI_CSI_RE.sub(b"", normalized)


class IdleHookSession:
    def __init__(self, binary: str, home: str) -> None:
        self.output = bytearray()
        self.query_tail = b""
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            env = os.environ.copy()
            env["HOME"] = home
            env["XDG_CONFIG_HOME"] = os.path.join(home, ".config")
            env["TERM"] = "xterm-256color"
            os.execve(
                binary,
                [
                    binary,
                    "--no-source",
                    "--no-titleline",
                    "--no-prompt-vars",
                    "--no-completions",
                    "--no-syntax-highlighting",
                    "--no-history",
                ],
                env,
            )  # nosemgrep

        flags = fcntl.fcntl(self.fd, fcntl.F_GETFL)
        fcntl.fcntl(self.fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def close(self) -> None:
        try:
            os.close(self.fd)
        except OSError:
            pass
        if self.pid > 0:
            try:
                os.kill(self.pid, signal.SIGKILL)
            except OSError:
                pass
            try:
                os.waitpid(self.pid, 0)
            except ChildProcessError:
                pass

    def pump(self, duration_s: float = 0.05) -> None:
        deadline = time.monotonic() + duration_s
        while time.monotonic() < deadline:
            try:
                chunk = os.read(self.fd, 4096)
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
            pending = self.query_tail + chunk
            for _ in range(pending.count(CURSOR_QUERY)):
                os.write(self.fd, CURSOR_RESPONSE)
            self.query_tail = pending[-(len(CURSOR_QUERY) - 1) :]

    def write(self, data: bytes) -> None:
        deadline = time.monotonic() + 4.0
        offset = 0
        while offset < len(data):
            try:
                written = os.write(self.fd, data[offset:])
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise AssertionError("timed out writing to the cjsh PTY")
                self.pump()
                continue

            if written == 0:
                raise AssertionError("cjsh PTY accepted a zero-length write")
            offset += written

    def wait_for(self, needle: bytes, start: int = 0, timeout_s: float = 4.0) -> int:
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            self.pump()
            index = self.output.find(needle, start)
            if index >= 0:
                return index
        raise AssertionError(
            f"missing {needle!r} in PTY output: {bytes(self.output[start:])!r}"
        )

    def wait_for_prompt(self, start: int, command_completed: bool = False) -> int:
        search_from = start
        if command_completed:
            output_end = self.wait_for(COMMAND_OUTPUT_END, start)
            search_from = output_end + len(COMMAND_OUTPUT_END)
        return self.wait_for(PROMPT_INPUT_START, search_from)

    def run_command(self, command: bytes) -> int:
        start = len(self.output)
        self.write(command + b"\r")
        self.wait_for_prompt(start, command_completed=True)
        return start

    def wait_for_exit(self, timeout_s: float = 4.0) -> int:
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            self.pump()
            waited, status = os.waitpid(self.pid, os.WNOHANG)
            if waited == self.pid:
                self.pump(0.1)
                self.pid = -1
                return os.waitstatus_to_exitcode(status)
        raise AssertionError("cjsh did not exit after the idle-hook test")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <cjsh>", file=sys.stderr)
        return 2

    binary = os.path.abspath(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix="cjsh-idle-hook-") as home:
        foreground_probe = os.path.join(home, "foreground-widget-probe")
        with open(foreground_probe, "w", encoding="utf-8") as probe_file:
            probe_file.write(
                "#!/bin/sh\n"
                "printf 'FOREGROUND-WIDGET\\n'\n"
                "IFS= read -r response </dev/tty\n"
                "printf 'echo FOREGROUND-%s' \"$response\" >&2\n"
            )
        os.chmod(foreground_probe, 0o755)

        session = IdleHookSession(binary, home)
        try:
            session.wait_for_prompt(0)
            session.run_command(b"cjshopt status-line off")

            session.run_command(
                b"preexec_probe() { printf 'PREEXEC-ARG:%s\\n' \"$1\"; }"
            )
            session.run_command(
                b"precmd_probe() { printf 'PRECMD-STATUS:%s:DURATION:%s\\n' "
                b'"$?" "$CJSH_COMMAND_DURATION_MS"; }'
            )
            session.run_command(b"hook add preexec preexec_probe")
            session.run_command(b"hook add precmd precmd_probe")
            hook_start = session.run_command(b"false")
            hook_output = normalize_terminal_output(bytes(session.output[hook_start:]))
            if b"PREEXEC-ARG:false" not in hook_output:
                raise AssertionError(
                    f"preexec hook did not receive the command: {hook_output!r}"
                )
            if re.search(rb"PRECMD-STATUS:1:DURATION:[0-9]+", hook_output) is None:
                raise AssertionError(
                    "precmd hook did not receive status and duration metadata: "
                    f"{hook_output!r}"
                )
            session.run_command(b"hook remove preexec preexec_probe")
            session.run_command(b"hook remove precmd precmd_probe")

            session.run_command(
                b"cursor_left_widget() { cjsh-widget action cursor-left; }"
            )
            session.run_command(b"cjshopt keybind ext set F6 cursor_left_widget")
            widget_start = len(session.output)
            session.write(b"echo AB\x1b[17~X\r")
            session.wait_for_prompt(widget_start, command_completed=True)
            widget_output = normalize_terminal_output(
                bytes(session.output[widget_start:])
            )
            if b"\nAXB\n" not in widget_output:
                raise AssertionError(
                    f"editor action did not preserve its cursor move: {widget_output!r}"
                )

            probe_command = (
                "foreground_widget() { local selection; "
                f'{shlex.quote(foreground_probe)} 2>"$HOME/widget-result"; '
                'selection=$(cat "$HOME/widget-result"); '
                'rm -f "$HOME/widget-result"; CJSH_LINE=$selection; }'
            )
            session.run_command(probe_command.encode())
            session.run_command(b"cjshopt keybind ext set F7 foreground_widget")
            foreground_start = len(session.output)
            session.write(b"discarded\x1b[18~")
            session.wait_for(b"FOREGROUND-WIDGET", foreground_start)
            session.write(b"OK\r")
            session.pump(0.2)
            session.write(b"\r")
            session.wait_for_prompt(foreground_start, command_completed=True)
            foreground_output = normalize_terminal_output(
                bytes(session.output[foreground_start:])
            )
            if b"\nFOREGROUND-OK\n" not in foreground_output:
                raise AssertionError(
                    "foreground external widget did not update the editor: "
                    f"{foreground_output!r}"
                )
            if b"Stopped" in foreground_output:
                raise AssertionError(
                    "foreground external widget was stopped by terminal job control"
                )

            probe = (
                b"idle_probe() { /bin/sh -c 'set -- $(ps -o tpgid= -o pgid= -p $$); "
                b'if [ "$1" = "$2" ]; then echo IDLE-FOREGROUND; '
                b"else echo IDLE-BACKGROUND; fi'; }"
            )
            session.run_command(probe)
            session.run_command(b"hook add idle idle_probe")
            session.run_command(b"cjshopt idle-timeout 1")
            session.run_command(b"prompt_probe() { echo PROMPT-RAN; }")
            prompt_command_start = session.run_command(b"PROMPT_COMMAND=prompt_probe")
            if b"PROMPT-RAN" not in session.output[prompt_command_start:]:
                raise AssertionError(
                    "PROMPT_COMMAND probe did not run before the idle test"
                )

            edit_start = len(session.output)
            session.write(b"echo AB\x1b[D\x1b[D")
            idle_marker = session.wait_for(
                b"IDLE-FOREGROUND", edit_start, timeout_s=3.0
            )
            if session.output.find(b"IDLE-BACKGROUND", edit_start) >= 0:
                raise AssertionError(
                    "idle hook command did not own the foreground terminal"
                )

            session.wait_for_prompt(idle_marker)
            if b"PROMPT-RAN" in session.output[edit_start:]:
                raise AssertionError(
                    "idle resume reran PROMPT_COMMAND for the same prompt"
                )
            submission_start = len(session.output)
            session.write(b"X\r")
            session.wait_for_prompt(submission_start, command_completed=True)

            normalized = normalize_terminal_output(bytes(session.output[edit_start:]))
            if b"XAB" not in normalized.splitlines():
                raise AssertionError(
                    "idle hook did not restore the pending input and cursor position: "
                    f"{normalized!r}"
                )

            session.run_command(b"cjshopt idle-timeout off")
            disabled_start = len(session.output)
            session.pump(1.3)
            if b"IDLE-FOREGROUND" in session.output[disabled_start:]:
                raise AssertionError("idle hook fired after idle-timeout was disabled")

            session.write(b"exit --force\r")
            result = session.wait_for_exit()
            if result != 0:
                raise AssertionError(f"cjsh exited with {result}, expected 0")
        finally:
            session.close()

    print("Interactive idle-hook integration test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
