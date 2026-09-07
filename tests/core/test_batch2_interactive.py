#!/usr/bin/env python3

# test_batch2_interactive.py
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

"""External terminal modes, nested suspension and session-private history drafts."""
import json
import os
from pathlib import Path
import shlex
import sys
import tempfile
import termios
import unittest

from test_idle_hook_interactive import IdleHookSession


class InteractiveTests(unittest.TestCase):
    binary: str

    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="cjsh-batch2-pty-")
        self.addCleanup(directory.cleanup)
        self.home = Path(directory.name)

    def session(self, history=False):
        args = [self.binary, "--no-config", "--no-titleline", "--no-prompt-vars",
                "--no-completions", "--no-syntax-highlighting"]
        if not history:
            args.append("--no-history")
        s = IdleHookSession(self.binary, str(self.home), argv=args)
        self.addCleanup(s.close)
        s.wait_for_prompt(0)
        return s

    def test_external_modes_persist_while_editor_remains_usable(self):
        session = self.session()
        probe = self.home / "term.py"
        result = self.home / "modes"
        probe.write_text('import termios,sys,json\na=termios.tcgetattr(0)\n'
                         'json.dump([*a[:4], [v if isinstance(v,int) else v[0] for v in a[6]]], open(sys.argv[1],"w"))\n')
        def modes():
            session.run_command(shlex.join([sys.executable, str(probe), str(result)]).encode())
            return json.loads(result.read_text())
        initial = modes()
        editor = termios.tcgetattr(session.fd)
        for command, field, flag in (("stty -isig", 3, termios.ISIG),
                                      ("stty -icrnl", 0, termios.ICRNL),
                                      ("stty -opost", 1, termios.OPOST)):
            session.run_command(command.encode())
            self.assertFalse(modes()[field] & flag)
            self.assertEqual(termios.tcgetattr(session.fd), editor)
        session.run_command(b"stty intr '^]' erase '^H' eof '^F'")
        observed = modes()
        self.assertEqual(observed[4][termios.VINTR], 29)
        self.assertEqual(observed[4][termios.VERASE], 8)
        self.assertEqual(observed[4][termios.VEOF], 6)
        session.run_command(b"sh -c 'stty raw -echo; kill -KILL $$'")
        self.assertEqual(modes(), observed)
        session.run_command(b"stty sane")
        self.assertTrue(modes()[3] & termios.ICANON)
        self.assertTrue(initial[3] & termios.ICANON)

    def test_nested_suspend_login_override_and_resume(self):
        for login in (False, True):
            with self.subTest(login=login):
                session = self.session()
                nested = [self.binary, "--no-config", "--no-titleline", "--no-prompt-vars", "--no-history"]
                if login:
                    nested += ["-l"]
                start = len(session.output)
                session.write(shlex.join(nested).encode() + b"\r")
                command_start = session.wait_for(b"\x1b]133;C", start)
                session.wait_for_prompt(command_start)
                child = os.tcgetpgrp(session.fd)
                self.assertNotEqual(child, session.pid)
                if login:
                    start = session.run_command(b"suspend")
                    self.assertIn(b"cannot suspend a login shell", session.output[start:])
                start = len(session.output)
                session.write(b"suspend -f\r" if login else b"suspend\r")
                session.wait_for_prompt(start, command_completed=True)
                self.assertEqual(os.tcgetpgrp(session.fd), session.pid)
                start = session.run_command(b"echo parent-usable")
                self.assertIn(b"parent-usable", session.output[start:])
                start = len(session.output)
                session.write(b"fg\r")
                session.wait_for_prompt(start, command_completed=True)
                self.assertEqual(os.tcgetpgrp(session.fd), child)
                session.run_command(b"echo child-usable")
                start = len(session.output)
                session.write(b"exit\r")
                session.wait_for_prompt(start, command_completed=True)
                self.assertEqual(os.tcgetpgrp(session.fd), session.pid)
                session.write(b"exit\r")
                self.assertEqual(session.wait_for_exit(), 0)

    def test_history_drafts_never_overwrite_other_sessions(self):
        first = self.session(history=True)
        second = self.session(history=True)
        path = self.home / ".cache/cjsh/history.txt"
        first.write(b"unfinished-secret-draft")
        first.pump(.1)
        self.assertNotIn("unfinished", path.read_text())
        second.run_command(b"echo second-committed")
        first.write(b"\x03")
        first.wait_for_prompt(len(first.output))
        first.run_command(b"echo first-committed")
        text = path.read_text()
        self.assertIn("echo second-committed", text)
        self.assertIn("echo first-committed", text)
        self.assertNotIn("unfinished", text)
        first.write(b"exit\r")
        second.write(b"exit\r")
        self.assertEqual(first.wait_for_exit(), 0)
        self.assertEqual(second.wait_for_exit(), 0)
        text = path.read_text()
        self.assertIn("echo second-committed", text)
        self.assertIn("echo first-committed", text)

    def test_no_config_keeps_history_enabled(self):
        session = self.session(history=True)
        session.run_command(b"echo persisted-without-config")
        path = self.home / ".cache/cjsh/history.txt"
        self.assertIn("echo persisted-without-config", path.read_text())
        session.run_command(b"cjshopt set-history-max 0")
        session.run_command(b"echo not-recorded")
        self.assertEqual(path.read_text(), "")
        session.run_command(b"cjshopt set-history-max 3")
        for number in range(5):
            session.run_command(f"echo retained-{number}".encode())
        commands = [line for line in path.read_text().splitlines() if not line.startswith("#")]
        self.assertEqual(commands, ["echo retained-2", "echo retained-3", "echo retained-4"])


if __name__ == "__main__":
    InteractiveTests.binary = str(Path(sys.argv[1]).resolve())
    unittest.main(argv=[sys.argv[0]], verbosity=2)
