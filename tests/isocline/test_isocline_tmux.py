#!/usr/bin/env python3

# test_isocline_tmux.py
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

import fcntl
import os
from pathlib import Path
import pty
import signal
import subprocess
import tempfile
import sys
import time

if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} <cjsh> <tmux>")
binary = str(Path(sys.argv[1]).resolve())
tmux_binary = str(Path(sys.argv[2]).resolve())
hook = (
    Path(__file__).resolve().parents[2] / "docs/examples/tmux-smart-mouse.conf"
).read_text()
# Use a short path: Unix-domain socket paths have a small length limit on macOS.
with tempfile.TemporaryDirectory(prefix="cjsh-mouse-release-", dir="/tmp") as directory:
    config = Path(directory) / "tmux.conf"
    config.write_text(
        "set -g remain-on-exit on\nset -g mouse on\nset -g status off\nset -g focus-events on\nset -g set-clipboard off\n"
        + hook
    )
    base = [tmux_binary, "-S", str(Path(directory) / "socket")]
    env = os.environ.copy()
    env.pop("TMUX", None)
    env["TERM"] = "xterm-256color"

    def tmux(*args):
        result = subprocess.run(
            base + list(args), env=env, text=True, capture_output=True
        )
        if result.returncode:
            raise RuntimeError(
                f"{args}: {result.stderr}; transcript={bytes(transcript)!r}"
            )
        return result.stdout.strip()

    pid = None
    fd = None
    transcript = bytearray()

    def drain():
        if fd is None:
            return
        while True:
            try:
                chunk = os.read(fd, 65536)
                if not chunk:
                    return
                transcript.extend(chunk)
            except (BlockingIOError, OSError):
                return

    def state():
        return tmux(
            "display-message",
            "-p",
            "-t",
            "test",
            "#{mouse_any_flag},#{mouse_sgr_flag},#{pane_in_mode},#{pane_current_command}",
        )

    def wait_for(predicate, label):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            drain()
            if predicate():
                return
            time.sleep(0.03)
        raise AssertionError(
            f"{label}: state={state()}, screen={tmux('capture-pane', '-p', '-t', 'test')!r}"
        )

    def mouse(code, column, row, release=False):
        os.write(fd, f"\x1b[<{code};{column};{row}{'m' if release else 'M'}".encode())
        time.sleep(0.06)
        drain()

    def input_line():
        row = int(tmux("display-message", "-p", "-t", "test", "#{cursor_y}"))
        lines = tmux("capture-pane", "-p", "-t", "test").splitlines()
        return lines[row] if row < len(lines) else ""

    def cancel_input():
        prompts = tmux("capture-pane", "-p", "-t", "test").count("cjsh>")
        tmux("send-keys", "-t", "test", "C-c")
        wait_for(
            lambda: tmux("capture-pane", "-p", "-t", "test").count("cjsh>") > prompts,
            "prompt after interrupt",
        )

    try:
        tmux(
            "-f",
            str(config),
            "new-session",
            "-d",
            "-x",
            "80",
            "-y",
            "24",
            "-s",
            "test",
            binary,
            "--secure",
            "--minimal",
            "-i",
        )
        pid, fd = pty.fork()
        if pid == 0:
            os.execve(base[0], base + ["attach-session", "-t", "test"], env)
        fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
        wait_for(lambda: "cjsh" in state(), "shell startup")
        wait_for(
            lambda: "cjsh>" in tmux("capture-pane", "-p", "-t", "test"), "first prompt"
        )
        tmux("send-keys", "-t", "test", "cjshopt mouse-clicking smart", "Enter")
        wait_for(lambda: state().startswith("1,1,0,"), "smart capture enabled")
        for mode_keys in ["emacs", "vi"]:
            tmux("set-window-option", "-t", "test", "mode-keys", mode_keys)
            cancel_input()
            tmux("send-keys", "-l", "-t", "test", "abcdefghij")
            wait_for(
                lambda: input_line().endswith("abcdefghij"),
                "input rendered",
            )
            col, row = map(
                int,
                tmux(
                    "display-message", "-p", "-t", "test", "#{cursor_x} #{cursor_y}"
                ).split(),
            )
            start = col - 10 + 1
            row += 1
            mouse(0, start, row)
            mouse(32, start + 2, row)
            wait_for(lambda: state().startswith("0,1,0,"), "capture suspended for drag")
            mouse(32, start + 4, row)
            wait_for(lambda: state().startswith("0,1,1,"), "tmux selecting")
            mouse(32, start + 7, row)
            mouse(0, start + 7, row, release=True)
            wait_for(
                lambda: state().startswith("1,1,0,"), "release restored smart capture"
            )
            copied = tmux("show-buffer")
            if not copied or copied not in "abcdefghij":
                raise AssertionError(f"copying must still work: {copied!r}")
            mouse(0, start, row)
            mouse(0, start, row, release=True)
            tmux("send-keys", "-l", "-t", "test", "X")
            wait_for(
                lambda: input_line().endswith("Xabcdefghij"),
                "next click positioned cursor",
            )

        cancel_input()
        prompts = tmux("capture-pane", "-p", "-t", "test").count("cjsh>")
        tmux("send-keys", "-t", "test", "cjshopt mouse-clicking all-off", "Enter")
        wait_for(
            lambda: tmux("capture-pane", "-p", "-t", "test").count("cjsh>") > prompts,
            "prompt after setting all-off",
        )
        wait_for(
            lambda: state().startswith("0,0,0,"),
            "all-off disables capture and release encoding",
        )
        tmux("send-keys", "-l", "-t", "test", "nopqrstuvw")
        wait_for(
            lambda: input_line().endswith("nopqrstuvw"),
            "all-off input rendered",
        )
        col, row = map(
            int,
            tmux(
                "display-message", "-p", "-t", "test", "#{cursor_x} #{cursor_y}"
            ).split(),
        )
        start = col - 10 + 1
        row += 1
        mouse(0, start, row)
        mouse(32, start + 2, row)
        wait_for(lambda: state().startswith("0,0,1,"), "all-off selection")
        mouse(32, start + 7, row)
        mouse(0, start + 7, row, release=True)
        wait_for(lambda: state().startswith("0,0,0,"), "hook respects all-off")
        print("PASS: tmux copy/release/click in emacs and vi modes; all-off preserved")
    finally:
        subprocess.run(base + ["kill-server"], env=env, capture_output=True)
        if fd is not None:
            os.close(fd)
        if pid:
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            os.waitpid(pid, 0)
