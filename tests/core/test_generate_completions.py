#!/usr/bin/env python3

# test_generate_completions.py
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
import struct
import subprocess
import sys
import tempfile
import termios
import time


FAKE_MAN = """#!/bin/sh

while [ "$#" -gt 0 ]; do
    if [ "$1" = "-P" ]; then
        shift 2
        continue
    fi
    target=$1
    break
done

case "$target" in
    parallel-root)
        printf 'NAME\\n    parallel-root - root command\\n\\nCOMMANDS\\n    alpha  Alpha command\\n    beta   Beta command\\n'
        ;;
    parallel-root-alpha|parallel-root-beta)
        name=${target##*-}
        other=alpha
        if [ "$name" = "alpha" ]; then
            other=beta
        fi
        : >"$CJSH_TEST_MAN_STATE/$name"
        count=0
        while [ ! -f "$CJSH_TEST_MAN_STATE/$other" ] && [ "$count" -lt 100 ]; do
            sleep 0.01
            count=$((count + 1))
        done
        [ -f "$CJSH_TEST_MAN_STATE/$other" ] || exit 1
        printf 'NAME\\n    %s - %s command\\n' "$target" "$name"
        ;;
    *)
        exit 1
        ;;
esac
"""


def run_in_pty(arguments: list[str], environment: dict[str, str]) -> tuple[int, bytes]:
    master_fd, slave_fd = os.openpty()
    fcntl.ioctl(slave_fd, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
    process = subprocess.Popen(
        arguments,
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
        env=environment,
        close_fds=True,
    )
    os.close(slave_fd)

    flags = fcntl.fcntl(master_fd, fcntl.F_GETFL)
    fcntl.fcntl(master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    output = bytearray()
    deadline = time.monotonic() + 8.0

    try:
        while time.monotonic() < deadline:
            try:
                chunk = os.read(master_fd, 4096)
            except BlockingIOError:
                chunk = b""
            except OSError as exc:
                if exc.errno == errno.EIO:
                    break
                raise

            if chunk:
                output.extend(chunk)
                continue
            if process.poll() is not None:
                break
            time.sleep(0.01)
        else:
            process.kill()
            raise AssertionError("generate-completions did not finish within 8 seconds")

        return process.wait(timeout=2.0), bytes(output)
    finally:
        os.close(master_fd)
        if process.poll() is None:
            process.kill()
            process.wait(timeout=2.0)


def read_cache(home: str, target: str) -> str:
    path = os.path.join(
        home, ".cache", "cjsh", "generated_completions", f"{target}.txt"
    )
    with open(path, encoding="utf-8") as cache_file:
        return cache_file.read()


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <cjsh>", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="cjsh-generate-completions-") as temp_dir:
        home = os.path.join(temp_dir, "home")
        state = os.path.join(temp_dir, "state")
        os.mkdir(home)
        os.mkdir(state)

        fake_man = os.path.join(temp_dir, "fake-man")
        with open(fake_man, "w", encoding="utf-8") as script:
            script.write(FAKE_MAN)
        os.chmod(fake_man, 0o755)

        environment = os.environ.copy()
        environment.update(
            {
                "CJSH_MAN_PATH": fake_man,
                "CJSH_TEST_MAN_STATE": state,
                "HOME": home,
                "TERM": "xterm-256color",
            }
        )

        status, output = run_in_pty(
            [
                sys.argv[1],
                "-c",
                "generate-completions --subcommands --jobs 2 parallel-root",
            ],
            environment,
        )

        if status != 0:
            raise AssertionError(
                f"generate-completions exited with {status}: {output!r}"
            )
        if b"0/3 0%" not in output or b"3/3 100%" not in output:
            raise AssertionError(
                f"subcommands were not included in progress totals: {output!r}"
            )

        alpha_cache = read_cache(home, "parallel-root-alpha")
        beta_cache = read_cache(home, "parallel-root-beta")
        if "summary: alpha command" not in alpha_cache:
            raise AssertionError(
                f"alpha subcommand did not run in the shared pool: {alpha_cache!r}"
            )
        if "summary: beta command" not in beta_cache:
            raise AssertionError(
                f"beta subcommand did not run in the shared pool: {beta_cache!r}"
            )

    print("generate-completions subcommand scheduling test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
