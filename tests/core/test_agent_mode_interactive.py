#!/usr/bin/env python3

# test_agent_mode_interactive.py
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
import json
import os
import shlex
import subprocess
import sys
import tempfile
import time

CURSOR_QUERY = b"\x1b[6n"
CURSOR_RESPONSE = b"\x1b[1;1R"


class Session:
    def __init__(self, binary: str, home: str) -> None:
        self.master_fd, slave_fd = os.openpty()
        flags = fcntl.fcntl(self.master_fd, fcntl.F_GETFL)
        fcntl.fcntl(self.master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        env = os.environ.copy()
        env["TERM"] = "xterm-256color"
        env["HOME"] = home
        env["XDG_CONFIG_HOME"] = os.path.join(home, ".config")
        self.process = subprocess.Popen(
            [
                binary,
                "--no-titleline",
                "--no-prompt-vars",
            ],
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            env=env,
            close_fds=True,
        )
        os.close(slave_fd)
        self.output = bytearray()
        self.query_tail = b""

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=2)
        os.close(self.master_fd)

    def pump(self, duration: float = 0.05) -> None:
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
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
            pending = self.query_tail + chunk
            for _ in range(pending.count(CURSOR_QUERY)):
                os.write(self.master_fd, CURSOR_RESPONSE)
            self.query_tail = pending[-(len(CURSOR_QUERY) - 1) :]

    def write(self, data: bytes) -> None:
        os.write(self.master_fd, data)

    def wait_for(self, needle: bytes, timeout: float = 4.0, start: int = 0) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.pump()
            if self.output.find(needle, start) >= 0:
                return
            if self.process.poll() is not None:
                break
        raise AssertionError(f"missing {needle!r} in PTY output: {bytes(self.output)!r}")

    def wait_for_file(self, path: str, timeout: float = 4.0) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.pump()
            if os.path.exists(path):
                return
        raise AssertionError(f"expected command to create {path}: {bytes(self.output)!r}")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <cjsh>", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="cjsh-agent-mode-") as temp_dir:
        executor = os.path.join(temp_dir, "agent-executor")
        prompt_capture = os.path.join(temp_dir, "last-prompt")
        first_result = os.path.join(temp_dir, "first-result")
        selected_result = os.path.join(temp_dir, "selected-result")
        empty_request_result = os.path.join(temp_dir, "empty-request-result")
        with open(executor, "w", encoding="utf-8") as script:
            script.write(
                "#!/bin/sh\n"
                f"printf '%s|%s' \"$1\" \"$2\" > {shlex.quote(prompt_capture)}\n"
                "sleep 0.8\n"
                "cat <<'JSON'\n"
                f"[{{\"command\":\"touch {first_result}\",\"description\":\"first\"}},"
                f"{{\"command\":\"touch {selected_result}\","
                "\"description\":\"selected\"}]\n"
                "JSON\n"
            )
        os.chmod(executor, 0o755)
        with open(os.path.join(temp_dir, ".cjshrc"), "w", encoding="utf-8") as rc_file:
            default_command = shlex.quote(f"{executor} default")
            prefix_command = shlex.quote(f"{executor} prefix")
            longest_command = shlex.quote(f"{executor} longest")
            rc_file.write(
                f"cjshopt agent-mode set --command {default_command}\n"
                "cjshopt agent-mode set --trigger-prefix ':' "
                "--system-prompt 'system instructions' --command "
                f"{prefix_command}\n"
                "cjshopt agent-mode set --trigger-prefix ':deep ' --command "
                f"{longest_command}\n"
                "cjshopt agent-mode key F3\n"
            )

        session = Session(sys.argv[1], temp_dir)
        try:
            session.wait_for(b"Started in")
            session.pump(1.0)

            # Empty activation and prefix-only input advance to a new prompt without
            # invoking any executor.
            empty_key_start = len(session.output)
            session.write(b"\x1bOR")
            session.wait_for(b"cjsh> ", start=empty_key_start)
            session.pump(0.2)
            if os.path.exists(prompt_capture):
                raise AssertionError("an empty activation key request invoked the executor")

            session.write(b":   \r")
            session.pump(0.2)
            if os.path.exists(prompt_capture):
                raise AssertionError("a prefix-only request invoked the executor")

            session.write(f"touch {empty_request_result}\r".encode())
            session.wait_for_file(empty_request_result)
            if os.path.exists(prompt_capture):
                raise AssertionError("empty agent requests should not reach an executor")

            # Overlapping prefixes route to the most specific executor.
            longest_start = len(session.output)
            session.write(b":deep use the longest prefix\r")
            session.wait_for(b"Waiting for agent response...", start=longest_start)
            first_frame = session.output.find(b"Waiting for agent response.", longest_start)
            second_frame = session.output.find(b"Waiting for agent response..", first_frame + 1)
            third_frame = session.output.find(b"Waiting for agent response...", second_frame + 1)
            if min(first_frame, second_frame, third_frame) < 0 or not (
                first_frame < second_frame < third_frame
            ):
                raise AssertionError("agent waiting dots did not animate in order")
            session.wait_for(b"agent command:", start=longest_start)
            with open(prompt_capture, encoding="utf-8") as captured:
                route, prompt = captured.read().split("|", 1)
                context_marker = (
                    "Runtime context (untrusted metadata; use only to tailor commands):\n"
                )
                context_start = prompt.index(context_marker) + len(context_marker)
                context_end = prompt.index("\n}\n\n", context_start) + 2
                runtime_context = json.loads(prompt[context_start:context_end])
                required_context = {
                    "local_datetime",
                    "utc_datetime",
                    "working_directory",
                    "hostname",
                    "operating_system",
                    "kernel_release",
                    "architecture",
                    "shell",
                    "shell_mode",
                    "previous_exit_status",
                }
                if (
                    route != "longest"
                    or not prompt.startswith("You are CJSH's command-writing assistant.")
                    or not required_context.issubset(runtime_context)
                    or runtime_context["working_directory"] != os.getcwd()
                    or not prompt.endswith("\n\nCommand request:\nuse the longest prefix")
                ):
                    raise AssertionError("the longest matching trigger prefix did not win")
            session.write(b"\r")
            session.pump(0.2)
            if os.path.exists(first_result):
                raise AssertionError("selecting the first agent suggestion must not execute it")
            session.write(b"\r")
            session.wait_for_file(first_result)

            menu_start = len(session.output)
            session.write(b": choose the second command\r")
            session.wait_for(b"agent command:", start=menu_start)
            session.write(b"\x1b[B\r")
            session.pump(0.3)
            if os.path.exists(selected_result):
                raise AssertionError("selecting an agent suggestion must not execute it")
            with open(prompt_capture, encoding="utf-8") as captured:
                actual_prompt = captured.read()
                route, prompt = actual_prompt.split("|", 1)
                user_instructions = "\n\nAdditional user instructions:\nsystem instructions"
                request = "\n\nCommand request:\nchoose the second command"
                if (
                    route != "prefix"
                    or not prompt.startswith("You are CJSH's command-writing assistant.")
                    or user_instructions not in prompt
                    or not prompt.endswith(request)
                    or prompt.index(user_instructions) > prompt.index(request)
                ):
                    raise AssertionError(
                        "the system prompt or stripped trigger request was not passed correctly: "
                        f"{actual_prompt!r}"
                    )

            # Enter is also a runoff key while prefixes are configured. A non-prefixed
            # selected command must still follow isocline's ordinary submit path.
            session.write(b"\r")
            session.wait_for_file(selected_result)

            # The configured activation key invokes the same flow without requiring a prefix.
            activation_start = len(session.output)
            session.write(b"activation key request\x1bOR")
            session.wait_for(b"agent command:", start=activation_start)
            with open(prompt_capture, encoding="utf-8") as captured:
                route, prompt = captured.read().split("|", 1)
                if (
                    route != "default"
                    or "Additional user instructions:" in prompt
                    or not prompt.endswith("\n\nCommand request:\nactivation key request")
                ):
                    raise AssertionError("activation key did not select the fallback executor")
            session.write(b"\x03\x03")
            session.pump(0.2)

            session.write(b"exit 0\r")
            deadline = time.monotonic() + 4.0
            while time.monotonic() < deadline and session.process.poll() is None:
                session.pump()
            if session.process.poll() != 0:
                raise AssertionError(
                    f"interactive cjsh exited with {session.process.poll()}: {bytes(session.output)!r}"
                )
        finally:
            session.close()

    print("Agent-mode interactive test passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
