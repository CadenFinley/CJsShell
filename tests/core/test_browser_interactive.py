#!/usr/bin/env python3

# test_browser_interactive.py
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

import json
import os
from pathlib import Path
import re
import shlex
import sys
import tempfile
from urllib.parse import quote

from test_agent_mode_interactive import PROMPT_INPUT_START, Session, normalize_terminal_output


BROWSER_KEY = b"\x1bo"
SNAPSHOT_KEY = b"\x1bOS"  # F4


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <cjsh>", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="cjsh-browser-") as temp_dir:
        root = Path(temp_dir)
        home = root / "home"
        home.mkdir()
        capture = root / "browser.json"
        snapshot = root / "snapshot"
        injected = root / "injected"
        launcher = root / "browser launcher"
        launcher.write_text(
            f"#!{sys.executable}\n"
            "import json, os, sys, termios\n"
            "state = termios.tcgetattr(0)\n"
            f"with open({str(capture)!r}, 'w') as output:\n"
            "    json.dump({'args': sys.argv[1:], 'canonical': bool(state[3] & termios.ICANON)}, output)\n"
            "sys.exit(7 if 'fail' in sys.argv else 0)\n",
            encoding="utf-8",
        )
        launcher.chmod(0o755)
        # Exercise the platform fallback without launching any real applications.
        bin_dir = root / "bin"
        bin_dir.mkdir()
        for name in ("open", "xdg-open"):
            (bin_dir / name).symlink_to(launcher)

        browser_command = f'{shlex.quote(str(launcher))} --new-window "profile name"'
        snapshot_command = (
            "{ cjsh-widget get-cursor; echo; cjsh-widget get-buffer; } > "
            + shlex.quote(str(snapshot))
        )
        snapshot_binding = f"cjshopt keybind ext set F4 {shlex.quote(snapshot_command)}"
        (home / ".cjshrc").write_text(
            f"export BROWSER={shlex.quote(browser_command)}\n"
            "unset CJSH_BROWSER_SEARCH_URL\n"
            f"export PATH={shlex.quote(str(bin_dir))}:$PATH\n"
            f"{snapshot_binding}\n",
            encoding="utf-8",
        )
        session = Session(os.path.abspath(sys.argv[1]), str(home), cwd=str(root))
        checks = 0

        def assert_snapshot(text: str, point: int) -> None:
            snapshot.unlink(missing_ok=True)
            start = len(session.output)
            session.write(SNAPSHOT_KEY)
            session.wait_for_file(str(snapshot))
            session.wait_for(b"\x1b[?2004h", start=start)
            actual = snapshot.read_text(encoding="utf-8")
            expected = f"{point}\n{text}"
            if actual != expected:
                raise AssertionError(f"editor changed: expected {expected!r}, got {actual!r}")

        def clear_input() -> None:
            start = len(session.output)
            session.write(b"\x03")
            session.wait_for_next_prompt(start)

        def open_text(text: str, expected_url: str, args: list[str] | None = None) -> None:
            nonlocal checks
            capture.unlink(missing_ok=True)
            session.paste(text.encode())
            # Starting inside the input must still preserve the whole request above
            # the fresh prompt and reset the cursor to the empty buffer's start.
            start = len(session.output)
            session.write(b"\x01" + BROWSER_KEY)
            session.wait_for_file(str(capture))
            session.wait_for(b"\x1b[?2004h", start=start)
            actual = json.loads(capture.read_text(encoding="utf-8"))
            expected_args = ["--new-window", "profile name"] if args is None else args
            if actual["args"] != expected_args + [expected_url]:
                raise AssertionError(f"wrong browser arguments: {actual!r}")
            if not actual["canonical"]:
                raise AssertionError("browser inherited the editor's raw terminal mode")
            assert_snapshot("", 0)
            checks += 1

        try:
            session.wait_for(PROMPT_INPUT_START)
            query = f"café & C++ #100% 'quotes' $(touch {injected}); `whoami`"
            open_text(query, "https://www.google.com/search?q=" + quote(query, safe=""))
            if injected.exists():
                raise AssertionError("browser request was executed as shell code")
            command_text = f"touch {injected}"
            open_text(command_text, "https://www.google.com/search?q=" + quote(command_text, safe=""))
            if injected.exists():
                raise AssertionError("advancing the browser prompt submitted the search as a command")
            for url in (
                "https://example.com/a?q=one&x=$HOME#fragment",
                "HTTP://example.com/path",
                "file:///tmp/test%20page.html",
            ):
                open_text("  " + url + "  ", url)
            open_text("www.example.com/page", "https://www.example.com/page")
            open_text("youtube.com", "https://www.google.com/search?q=youtube.com")
            open_text("first line\nsecond line", "https://www.google.com/search?q=first%20line%0Asecond%20line")

            for empty in ("", "   "):
                capture.unlink(missing_ok=True)
                if empty:
                    session.paste(empty.encode())
                session.write(BROWSER_KEY)
                # ESC o also starts a legacy terminal sequence; allow its timeout
                # before sending another escape sequence after a no-op action.
                session.pump(0.1)
                assert_snapshot(empty, len(empty))
                if capture.exists():
                    raise AssertionError("empty buffer launched a browser")
                clear_input()
                checks += 1

            session.run_command(b"CJSH_BROWSER_SEARCH_URL='https://search.example/?text='")
            open_text("a+b", "https://search.example/?text=a%2Bb")
            session.run_command(b"unset CJSH_BROWSER_SEARCH_URL; unset BROWSER")
            open_text("fallback", "https://www.google.com/search?q=fallback", [])
            session.run_command(f"BROWSER={shlex.quote(browser_command)}".encode())

            for command in (
                "cjshopt keybind profile set vim",
                "cjshopt keybind profile set emacs",
                "cjshopt keybind reset",
            ):
                session.run_command(command.encode())
                # Keymap resets also remove our test-only snapshot binding.
                session.run_command(snapshot_binding.encode())
                open_text("after reset", "https://www.google.com/search?q=after%20reset")

            session.paste(b"hello world")
            session.write(b"\x1bb")
            assert_snapshot("hello world", 5)
            clear_input()
            checks += 1

            # User commands win, and clearing one restores the browser binding.
            session.run_command(
                f"cjshopt keybind ext set alt+o {shlex.quote(snapshot_command)}".encode()
            )
            capture.unlink(missing_ok=True)
            snapshot.unlink(missing_ok=True)
            start = len(session.output)
            session.enter_text(b"custom key", BROWSER_KEY)
            session.wait_for_file(str(snapshot))
            session.wait_for(b"\x1b[?2004h", start=start)
            if capture.exists() or snapshot.read_text() != "10\ncustom key":
                raise AssertionError("custom Alt+O binding did not take precedence")
            clear_input()
            session.run_command(b"cjshopt keybind ext clear alt+o")
            open_text("restored", "https://www.google.com/search?q=restored")

            # Reapplying shell bindings must also respect ordinary editing actions.
            session.run_command(b"cjshopt keybind add cursor-left alt+o")
            session.run_command(b"cjshopt keybind ext reset")
            session.run_command(snapshot_binding.encode())
            capture.unlink(missing_ok=True)
            session.enter_text(b"override", BROWSER_KEY)
            session.pump(0.1)
            assert_snapshot("override", 7)
            if capture.exists():
                raise AssertionError("browser replaced an explicit editing action")
            clear_input()
            session.run_command(b"cjshopt keybind reset")
            session.run_command(snapshot_binding.encode())
            open_text("reset again", "https://www.google.com/search?q=reset%20again")

            # Browser palette entry remains available with agent mode disabled.
            session.run_command(b"cjshopt agent-mode off")
            capture.unlink(missing_ok=True)
            start = len(session.output)
            session.enter_text(b"palette query", b"\x1bp")
            session.wait_for(b"command palette:", start=start)
            session.write(b"Open buffer in browser")
            session.wait_for(b"Open buffer in browser", start=start)
            session.write(b"\r")
            session.wait_for_file(str(capture))
            session.wait_for(b"\x1b[?2004h", start=start)
            if json.loads(capture.read_text())["args"][-1] != "https://www.google.com/search?q=palette%20query":
                raise AssertionError("palette did not open the original buffer")
            assert_snapshot("", 0)
            checks += 1

            for bad_command, error in (
                (f"{shlex.quote(str(launcher))} fail", b"Browser command exited with status 7"),
                (str(root / "missing-browser"), b"Browser command exited with status 127"),
                ("   ", b"BROWSER contains no command"),
            ):
                session.run_command(f"BROWSER={shlex.quote(bad_command)}".encode())
                start = len(session.output)
                session.enter_text(b"clear on failure", BROWSER_KEY)
                session.wait_for_normalized(error, start=start)
                session.wait_for(b"\x1b[?2004h", start=start)
                output = normalize_terminal_output(bytes(session.output[start:]))
                if b"clear on failure\ncjsh> " not in output:
                    raise AssertionError("failed request was not preserved above the fresh prompt")
                assert_snapshot("", 0)
                checks += 1
            session.run_command(f"BROWSER={shlex.quote(browser_command)}".encode())
            open_text("recovered", "https://www.google.com/search?q=recovered")
        finally:
            session.close()

        with (home / ".cjshrc").open("a", encoding="utf-8") as rc_file:
            rc_file.write("export PS1='normal> '\nexport PS1_FINAL='final> '\n")
        session = Session(os.path.abspath(sys.argv[1]), str(home), cwd=str(root), prompt_vars=True)
        try:
            session.wait_for(PROMPT_INPUT_START)
            start = len(session.output)
            open_text("styled search", "https://www.google.com/search?q=styled%20search")
            output = normalize_terminal_output(bytes(session.output[start:]))
            if re.search(rb"final> styled search\n+normal> ", output) is None:
                raise AssertionError(
                    f"browser request did not use the configured final prompt: {output!r}"
                )
        finally:
            session.close()

    print(f"All {checks} browser interactive tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
