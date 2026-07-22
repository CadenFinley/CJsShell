#!/usr/bin/env python3

# test_function_pipeline_job_control.py
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
import os
import pty
import re
import signal
import subprocess
import sys
import time
from typing import Callable, NamedTuple


class JobControlResult(NamedTuple):
    return_code: int | None
    output: str
    timed_out: bool


def sanitize_output(text: str) -> str:
    csi = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
    osc = re.compile(r"\x1b\][^\x07]*(?:\x07|\x1b\\)")
    text = osc.sub("", text)
    text = csi.sub("", text)
    return text.replace("\r", "\n")


def run_job_control_case(
    binary: str,
    command: str = (
        "stty tostop; "
        "f(){ sleep 0.01; printf 'tick\\n' >&2; /bin/echo target; }; "
        "f | grep target"
    ),
) -> JobControlResult:
    master_fd, slave_fd = os.openpty()
    os.set_blocking(master_fd, False)
    env = os.environ.copy()
    env.setdefault("TERM", "xterm-256color")

    process: subprocess.Popen[bytes] | None = None
    output = bytearray()
    timed_out = False

    try:
        process = subprocess.Popen(
            [binary, "--no-source", "--no-titleline", "--minimal", "-i", "-c", command],
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            env=env,
            close_fds=True,
        )
        os.close(slave_fd)
        slave_fd = -1

        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            try:
                chunk = os.read(master_fd, 65536)
            except BlockingIOError:
                chunk = b""
            except OSError as exc:
                if exc.errno == errno.EIO:
                    break
                raise

            if chunk:
                output.extend(chunk)
            elif process.poll() is not None:
                break
            else:
                time.sleep(0.02)

        if process.poll() is None:
            timed_out = True
    finally:
        if slave_fd >= 0:
            try:
                os.close(slave_fd)
            except OSError:
                pass
        try:
            os.close(master_fd)
        except OSError:
            pass

        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=2.0)

    if process is None:
        raise AssertionError("failed to start cjsh")

    cleaned = sanitize_output(output.decode(errors="replace"))
    return JobControlResult(process.returncode, cleaned, timed_out)


def run_noninteractive_terminal_ownership_case(binary: str) -> JobControlResult:
    pid, master_fd = pty.fork()
    if pid == 0:
        before_pgid = os.getpgrp()
        before_foreground_pgid = os.tcgetpgrp(0)

        nested_pid = os.fork()
        if nested_pid == 0:
            os.execl(
                binary,
                binary,
                "--no-source",
                "--no-titleline",
                "--minimal",
                "-c",
                "true",
            )

        _, nested_status = os.waitpid(nested_pid, 0)
        after_foreground_pgid = os.tcgetpgrp(0)
        nested_return_code = os.waitstatus_to_exitcode(nested_status)
        preserved = (
            nested_return_code == 0
            and before_foreground_pgid == before_pgid
            and after_foreground_pgid == before_pgid
        )
        message = (
            f"caller_pgid={before_pgid} "
            f"foreground_before={before_foreground_pgid} "
            f"foreground_after={after_foreground_pgid} "
            f"nested_exit={nested_return_code}\n"
        )
        os.write(1, message.encode())
        os._exit(0 if preserved else 1)

    os.set_blocking(master_fd, False)
    output = bytearray()
    wait_status: int | None = None
    timed_out = False

    try:
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                chunk = os.read(master_fd, 4096)
            except BlockingIOError:
                chunk = b""
            except OSError as exc:
                if exc.errno == errno.EIO:
                    chunk = b""
                else:
                    raise

            if chunk:
                output.extend(chunk)

            waited_pid, status = os.waitpid(pid, os.WNOHANG | os.WUNTRACED)
            if waited_pid == pid:
                wait_status = status
                break
            time.sleep(0.01)

        if wait_status is None:
            timed_out = True
            os.kill(pid, signal.SIGKILL)
            _, wait_status = os.waitpid(pid, 0)
        elif os.WIFSTOPPED(wait_status):
            stop_signal = os.WSTOPSIG(wait_status)
            output.extend(f"caller stopped by signal {stop_signal}\n".encode())
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)

        while True:
            try:
                chunk = os.read(master_fd, 4096)
            except BlockingIOError:
                break
            except OSError as exc:
                if exc.errno == errno.EIO:
                    break
                raise
            if not chunk:
                break
            output.extend(chunk)
    finally:
        os.close(master_fd)

    return_code = None
    if wait_status is not None and not os.WIFSTOPPED(wait_status):
        return_code = os.waitstatus_to_exitcode(wait_status)

    cleaned = sanitize_output(output.decode(errors="replace"))
    return JobControlResult(return_code, cleaned, timed_out)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def run_checks(checks: list[tuple[str, Callable[[], None]]], suite_name: str) -> int:
    failures = 0
    for label, check in checks:
        try:
            check()
        except Exception as exc:
            failures += 1
            message = f"{type(exc).__name__}: {exc}".replace("\n", "\n    ")
            print(f"FAIL: {label}: {message}", file=sys.stderr)
        else:
            print(f"PASS: {label}")

    if failures:
        print(f"{failures}/{len(checks)} {suite_name} tests failed", file=sys.stderr)
        return 1

    print(f"All {len(checks)} {suite_name} tests passed")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print("usage: test_function_pipeline_job_control.py <cjsh_binary>", file=sys.stderr)
        return 2

    try:
        result = run_job_control_case(argv[0])
        abort_result = run_job_control_case(argv[0], "sh -c 'kill -ABRT $$'")
        pipeline_abort_result = run_job_control_case(
            argv[0], "printf ignored | sh -c 'kill -ABRT $$'"
        )
        term_result = run_job_control_case(argv[0], "sh -c 'kill -TERM $$'")
        kill_result = run_job_control_case(argv[0], "sh -c 'kill -KILL $$'")
        background_term_result = run_job_control_case(
            argv[0], "sh -c 'kill -TERM $$' & sleep 0.1"
        )
        background_exit_result = run_job_control_case(
            argv[0], "sh -c 'exit 42' & sleep 0.1"
        )
        foreground_exit_result = run_job_control_case(argv[0], "sh -c 'exit 42'")
        ownership_result = run_noninteractive_terminal_ownership_case(argv[0])
    except Exception as exc:
        message = f"{type(exc).__name__}: {exc}".replace("\n", "\n    ")
        print(
            f"FAIL: function pipeline command setup: {message}",
            file=sys.stderr,
        )
        print("1/1 function pipeline job-control tests failed", file=sys.stderr)
        return 1

    checks: list[tuple[str, Callable[[], None]]] = [
        (
            "function pipeline command completes",
            lambda: require(
                not result.timed_out,
                f"cjsh did not finish the function pipeline command in time:\n{result.output}",
            ),
        ),
        (
            "function pipeline exits successfully",
            lambda: require(
                result.return_code == 0,
                f"cjsh exited with {result.return_code}:\n{result.output}",
            ),
        ),
        (
            "function pipeline stays in foreground",
            lambda: require(
                "Stopped" not in result.output,
                f"pipeline job unexpectedly stopped:\n{result.output}",
            ),
        ),
        (
            "function pipeline produces grep output",
            lambda: require(
                "target" in result.output,
                f"expected grep output was missing:\n{result.output}",
            ),
        ),
        (
            "non-interactive cjsh preserves caller terminal ownership",
            lambda: require(
                not ownership_result.timed_out and ownership_result.return_code == 0,
                "non-interactive cjsh changed its caller's foreground process group:\n"
                f"{ownership_result.output}",
            ),
        ),
        (
            "foreground SIGABRT preserves signal exit status",
            lambda: require(
                not abort_result.timed_out
                and abort_result.return_code == 128 + signal.SIGABRT,
                "SIGABRT command returned the wrong status:\n"
                f"return_code={abort_result.return_code}\n{abort_result.output}",
            ),
        ),
        (
            "foreground SIGABRT reports Abort",
            lambda: require(
                "Abort (SIGABRT)" in abort_result.output
                and abort_result.output.count("SIGABRT") == 1,
                "SIGABRT diagnostic was missing or duplicated:\n"
                f"{abort_result.output}",
            ),
        ),
        (
            "foreground pipeline SIGABRT preserves status and diagnostic",
            lambda: require(
                not pipeline_abort_result.timed_out
                and pipeline_abort_result.return_code == 128 + signal.SIGABRT
                and "Abort" in pipeline_abort_result.output,
                "pipeline SIGABRT handling failed:\n"
                f"return_code={pipeline_abort_result.return_code}\n"
                f"{pipeline_abort_result.output}",
            ),
        ),
        (
            "foreground SIGTERM uses centralized signal notification",
            lambda: require(
                not term_result.timed_out
                and term_result.return_code == 128 + signal.SIGTERM
                and term_result.output.count("SIGTERM") == 1,
                "SIGTERM handling failed or reported more than once:\n"
                f"return_code={term_result.return_code}\n{term_result.output}",
            ),
        ),
        (
            "foreground SIGKILL uses centralized signal notification",
            lambda: require(
                not kill_result.timed_out
                and kill_result.return_code == 128 + signal.SIGKILL
                and kill_result.output.count("SIGKILL") == 1,
                "SIGKILL handling failed or reported more than once:\n"
                f"return_code={kill_result.return_code}\n{kill_result.output}",
            ),
        ),
        (
            "background signal notification is emitted once",
            lambda: require(
                not background_term_result.timed_out
                and background_term_result.return_code == 0
                and background_term_result.output.count("SIGTERM") == 1,
                "background SIGTERM notification failed:\n"
                f"return_code={background_term_result.return_code}\n"
                f"{background_term_result.output}",
            ),
        ),
        (
            "background nonzero exit notification is emitted once",
            lambda: require(
                not background_exit_result.timed_out
                and background_exit_result.return_code == 0
                and background_exit_result.output.count("Exit 42") == 1,
                "background exit notification failed:\n"
                f"return_code={background_exit_result.return_code}\n"
                f"{background_exit_result.output}",
            ),
        ),
        (
            "foreground nonzero exit stays silent",
            lambda: require(
                not foreground_exit_result.timed_out
                and foreground_exit_result.return_code == 42
                and "Exit 42" not in foreground_exit_result.output,
                "foreground exit was reported as a background job:\n"
                f"return_code={foreground_exit_result.return_code}\n"
                f"{foreground_exit_result.output}",
            ),
        ),
    ]

    return run_checks(checks, "function pipeline job-control")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
