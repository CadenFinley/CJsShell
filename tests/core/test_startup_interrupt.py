#!/usr/bin/env python3

# test_startup_interrupt.py
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

"""Cancel startup through a real terminal and verify the resulting prompt."""

from __future__ import annotations

import os
from pathlib import Path
import shlex
import signal
import sys
import tempfile
import termios
import unittest
from unittest.mock import patch

from test_idle_hook_interactive import IdleHookSession, normalize_terminal_output


class StartupInterruptTests(unittest.TestCase):
    binary: str

    def session(self, stage: str, script: str, *args: str,
                files: dict[str, str] | None = None) -> tuple[IdleHookSession, Path]:
        directory = tempfile.TemporaryDirectory(prefix="cjsh-startup-interrupt-")
        self.addCleanup(directory.cleanup)
        home = Path(directory.name)
        marker = home / ".cache/cjsh/.first_boot"
        marker.parent.mkdir(parents=True)
        marker.touch()
        for name in (".cjshenv", ".cjprofile", ".cjshrc"):
            (home / name).write_text('echo ' + name + ' >> "$HOME/stages"\n')
        (home / stage).write_text(script + '\necho continued >> "$HOME/stages"\n')
        for name, content in (files or {}).items():
            (home / name).write_text(content)
        env = {"CJSH_ENV": "", "CJSH_CONFIG_HOME": "", "ENV": str(home / "posix-env")}
        if stage == "override-env":
            env["CJSH_ENV"] = str(home / stage)
        with patch.dict(os.environ, env):
            session = IdleHookSession(self.binary, str(home), argv=[
                self.binary, "--config-dir", str(home), "--no-titleline", "--no-history",
                "--no-agent", "--no-prompt-vars", "--no-completions",
                "--no-syntax-highlighting", *args,
            ])
        self.addCleanup(session.close)

        def cleanup_foreground() -> None:
            # A failed assertion must not leave the slow fixture running.
            if session.pid > 0:
                try:
                    pgid = os.tcgetpgrp(session.fd)
                    if pgid > 0 and pgid != session.pid:
                        os.killpg(pgid, signal.SIGKILL)
                except OSError:
                    pass

        self.addCleanup(cleanup_foreground)
        return session, home

    def assert_cancelled(self, session: IdleHookSession, home: Path,
                         expected_stages: str = "", *, signal_shell: bool = False) -> None:
        session.wait_for(b"STARTUP-READY\r\n")
        start = len(session.output)
        if signal_shell:
            os.kill(session.pid, signal.SIGINT)
        else:
            session.write(b"\x03")
        session.wait_for_prompt(start)
        self.assertEqual(os.tcgetpgrp(session.fd), session.pid)
        start = session.run_command(b"printf 'status=%s\\n' \"$?\"")
        self.assertIn(b"\nstatus=130\n", normalize_terminal_output(bytes(session.output[start:])))
        stages = home / "stages"
        self.assertEqual(stages.read_text() if stages.exists() else "", expected_stages)
        child = home / "child-pid"
        if child.exists():
            with self.assertRaises(ProcessLookupError):
                os.kill(int(child.read_text()), 0)
        start = session.run_command(b"printf 'prompt-%s\\n' working")
        self.assertIn(b"\nprompt-working\n", normalize_terminal_output(bytes(session.output[start:])))
        session.write(b"\x04")
        self.assertEqual(session.wait_for_exit(), 0)
        modes = termios.tcgetattr(session.fd)
        self.assertTrue(modes[3] & termios.ICANON)
        self.assertTrue(modes[3] & termios.ECHO)

    @staticmethod
    def slow_command() -> str:
        return "sh -c " + shlex.quote(
            'echo $$ > "$HOME/child-pid"; printf "STARTUP-READY\\n"; exec sleep 30')

    def test_slow_command_skips_remaining_startup_files(self) -> None:
        for stage, previous, args in (
            (".cjshenv", "", ("-l",)),
            (".cjprofile", ".cjshenv\n", ("-l",)),
            (".cjshrc", ".cjshenv\n.cjprofile\n", ("-l",)),
            ("override-env", "", ("-l",)),
            ("posix-env", "", ("--posix",)),
        ):
            with self.subTest(stage=stage):
                session, home = self.session(stage, self.slow_command(), *args)
                self.assert_cancelled(session, home, previous)

    def test_builtin_loops_unwind(self) -> None:
        for loop in (
            "while :; do :; done",
            "until false; do :; done",
            "spin() { while :; do :; done; }; spin",
        ):
            with self.subTest(loop=loop):
                session, home = self.session(".cjshenv", "echo STARTUP-READY\n" + loop, "-l")
                self.assert_cancelled(session, home)

    def test_nested_source_and_logical_commands_unwind(self) -> None:
        session, home = self.session(".cjshenv", '. "$HOME/nested" || echo recovered', "-l",
                                     files={"nested": self.slow_command() +
                                            '\necho nested >> "$HOME/stages"\n'})
        self.assert_cancelled(session, home)

    def test_pipeline_interrupt_survives_successful_last_command(self) -> None:
        # The last pipeline member exits normally; another member receives SIGINT.
        session, home = self.session(".cjshenv", self.slow_command() +
                                     " | sh -c 'trap \"\" INT; exec cat'", "-l")
        self.assert_cancelled(session, home)

    def test_read_is_interruptible(self) -> None:
        for command in ("read value", "read -t 30 value"):
            with self.subTest(command=command):
                session, home = self.session(".cjshenv", "echo STARTUP-READY\n" + command, "-l")
                self.assert_cancelled(session, home)

    def test_command_substitution_is_interruptible(self) -> None:
        command = self.slow_command().replace('printf "STARTUP-READY\\n"',
                                             'printf "STARTUP-READY\\n" >&2')
        for template in ("value=$(%s)", "printf 'expanded %%s\\n' \"$(%s)\""):
            with self.subTest(template=template):
                session, home = self.session(".cjshenv", template % command, "-l")
                self.assert_cancelled(session, home)
                self.assertNotIn(b"expanded", session.output)

    def test_signal_to_shell_interrupts_foreground_child(self) -> None:
        session, home = self.session(".cjshenv", self.slow_command(), "-l")
        self.assert_cancelled(session, home, signal_shell=True)

    def test_interrupt_trap_runs_before_unwinding(self) -> None:
        session, home = self.session(".cjshenv",
            "trap 'echo trapped >> \"$HOME/stages\"' INT\n"
            "echo STARTUP-READY\nwhile :; do :; done", "-l")
        self.assert_cancelled(session, home, "trapped\n")

    def test_errexit_does_not_prevent_prompt_recovery(self) -> None:
        session, home = self.session(".cjshenv", "set -e\n" + self.slow_command(), "-l")
        self.assert_cancelled(session, home)

    def test_interrupted_command_restores_terminal_modes(self) -> None:
        command = self.slow_command().replace("echo $$", "stty -echo -icanon; echo $$")
        for suffix in ("", " | sh -c 'trap \"\" INT; exec cat'"):
            with self.subTest(suffix=suffix):
                session, home = self.session(".cjshenv", command + suffix, "-l")
                self.assert_cancelled(session, home)

    def test_interactive_command_body_is_skipped_after_cancellation(self) -> None:
        session, home = self.session(".cjshenv", self.slow_command(), "-ilc",
                                     'echo body >> "$HOME/stages"')
        session.wait_for(b"STARTUP-READY\r\n")
        session.write(b"\x03")
        self.assertEqual(session.wait_for_exit(), 130)
        self.assertFalse((home / "stages").exists())

    def test_ctrl_d_does_not_cancel_startup(self) -> None:
        session, home = self.session(".cjshenv", "echo STARTUP-READY\nread value", "-l")
        session.wait_for(b"STARTUP-READY\r\n")
        session.write(b"\x04")
        session.wait_for_prompt(0)
        self.assertEqual((home / "stages").read_text(), "continued\n.cjprofile\n.cjshrc\n")
        session.write(b"\x04")
        self.assertEqual(session.wait_for_exit(), 0)

    def test_ordinary_status_130_does_not_cancel_other_startup_files(self) -> None:
        session, home = self.session(".cjshenv", "sh -c 'exit 130'", "-l")
        session.wait_for_prompt(0)
        self.assertEqual((home / "stages").read_text(), ".cjprofile\n.cjshrc\n")
        session.write(b"\x04")
        self.assertEqual(session.wait_for_exit(), 0)


if __name__ == "__main__":
    StartupInterruptTests.binary = os.path.abspath(sys.argv.pop(1))
    unittest.main()
