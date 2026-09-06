#!/usr/bin/env python3

# test_terminal_recovery.py
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

"""Exercise the real prompt after foreground programs damage terminal state."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import signal
import sys
import tempfile
import termios
import unittest

from test_idle_hook_interactive import IdleHookSession, normalize_terminal_output


class TerminalRecoveryTests(unittest.TestCase):
    binary: str

    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory(prefix="cjsh-terminal-recovery-")
        self.addCleanup(self.directory.cleanup)
        self.session = IdleHookSession(self.binary, self.directory.name)
        self.addCleanup(self.session.close)
        self.session.wait_for_prompt(0)
        self.editor_modes = termios.tcgetattr(self.session.fd)

    def assert_prompt_recovered(self) -> None:
        self.assertEqual(os.tcgetpgrp(self.session.fd), self.session.pid)
        self.assertEqual(termios.tcgetattr(self.session.fd), self.editor_modes)
        start = len(self.session.output)
        # Exercise ordinary typing, Backspace, Left and Delete after recovery.
        self.session.write(b"printf 'editing-%s\\n' oX\x7fkX\x1b[D\x1b[3~\r")
        self.session.wait_for_prompt(start, command_completed=True)
        output = normalize_terminal_output(bytes(self.session.output[start:]))
        self.assertIn(b"editing-ok", output)

    def track_job(self, pid_file: Path) -> None:
        def cleanup() -> None:
            if pid_file.exists():
                for pid in pid_file.read_text().split():
                    try:
                        os.kill(int(pid), signal.SIGKILL)
                    except ProcessLookupError:
                        pass

        self.addCleanup(cleanup)

    def test_raw_terminal_after_exit_or_kill(self) -> None:
        for ending in ("exit 0", "exit 7", "kill -KILL $$"):
            with self.subTest(ending=ending):
                command = "sh -c " + shlex.quote("stty raw -echo; " + ending)
                self.session.run_command(command.encode())
                self.assert_prompt_recovered()

    def test_monitor_off_recovers_before_precmd(self) -> None:
        probe = Path(self.directory.name) / "probe_terminal.py"
        result = Path(self.directory.name) / "terminal-state.json"
        probe.write_text("""
import json
import os
import sys
import termios
with open(sys.argv[1], 'w') as output:
    json.dump([os.tcgetpgrp(0), termios.tcgetattr(0)[:6]], output)
""", encoding="utf-8")
        self.session.run_command(b"set +m")
        probe_command = shlex.join([sys.executable, str(probe), str(result)])
        self.session.run_command(
            ("terminal_probe() { " + probe_command + "; }; hook add precmd terminal_probe").encode()
        )
        expected = json.loads(result.read_text())
        self.assertEqual(expected[0], self.session.pid)

        thief = Path(self.directory.name) / "steal_terminal_and_exit.py"
        thief.write_text("""
import os
import signal
import tty
os.setpgid(0, 0)
signal.signal(signal.SIGTTOU, signal.SIG_IGN)
os.tcsetpgrp(0, os.getpgrp())
tty.setraw(0)
""", encoding="utf-8")
        for command in (
            "sh -c 'stty raw -echo; exit 7'",
            shlex.join([sys.executable, str(thief)]),
        ):
            with self.subTest(command=command):
                result.unlink()
                self.session.run_command(command.encode())
                self.assertEqual(json.loads(result.read_text()), expected)
                self.assert_prompt_recovered()

    def test_recovery_preserves_queued_input(self) -> None:
        self.session.run_command(b"set +m")
        start = len(self.session.output)
        command = b"sh -c 'stty -echo; printf \"capture-ready\\n\"; sleep 0.3'"
        self.session.write(b"\x1b[200~" + command + b"\x1b[201~\r")
        self.session.wait_for(b"capture-ready\r\n", start)
        self.session.write(b"printf 'queued-%s\\n' ok")
        self.session.wait_for_prompt(start, command_completed=True)
        start = len(self.session.output)
        self.session.write(b"\r")
        self.session.wait_for_prompt(start, command_completed=True)
        output = normalize_terminal_output(bytes(self.session.output[start:]))
        self.assertIn(b"\nqueued-ok\n", output)
        self.assert_prompt_recovered()

    def test_raw_terminal_preserves_queued_return(self) -> None:
        self.session.run_command(b"set +m")
        start = len(self.session.output)
        command = b"sh -c 'stty raw -echo; printf \"raw-ready\\n\"; sleep 0.3'"
        self.session.write(b"\x1b[200~" + command + b"\x1b[201~\r")
        self.session.wait_for(b"raw-ready\n", start)
        self.session.write(b"printf 'raw-queued-%s\\n' ok\r")
        output_start = self.session.wait_for(b"raw-queued-ok\r\n", start)
        self.session.wait_for_prompt(output_start, command_completed=True)
        self.assert_prompt_recovered()

    def test_stopped_raw_job_resumes_with_its_saved_modes(self) -> None:
        script = Path(self.directory.name) / "stopped_raw_job.py"
        pid_file = Path(self.directory.name) / "stopped-job-pid"
        self.track_job(pid_file)
        script.write_text("""
import os
import signal
import sys
import termios
import tty
with open(sys.argv[1], 'w') as output:
    output.write(str(os.getpid()))
tty.setraw(0)
job_modes = termios.tcgetattr(0)
os.kill(os.getpid(), signal.SIGSTOP)
assert termios.tcgetattr(0) == job_modes, 'fg did not restore job modes'
print('job-modes-preserved', flush=True)
os.unlink(sys.argv[1])
""", encoding="utf-8")
        self.session.run_command(shlex.join([sys.executable, str(script), str(pid_file)]).encode())
        self.assert_prompt_recovered()
        start = self.session.run_command(b"fg")
        output = normalize_terminal_output(bytes(self.session.output[start:]))
        self.assertIn(b"job-modes-preserved", output)
        self.assert_prompt_recovered()

    def test_descendant_steals_terminal_and_stops_foreground_job(self) -> None:
        script = Path(self.directory.name) / "steal_terminal.py"
        child_file = Path(self.directory.name) / "child-pid"
        self.track_job(child_file)
        script.write_text("""
import os
import signal
import sys
import time
import tty
child = os.fork()
if child == 0:
    os.setpgid(0, 0)
    with open(sys.argv[1], 'w') as output:
        output.write(f'{os.getppid()} {os.getpid()}')
    signal.signal(signal.SIGTTOU, signal.SIG_IGN)
    os.tcsetpgrp(0, os.getpgrp())
    tty.setraw(0)
    os.kill(os.getppid(), signal.SIGSTOP)
    time.sleep(0.2)
    os._exit(0)
os.waitpid(child, 0)
os.unlink(sys.argv[1])
""", encoding="utf-8")
        command = shlex.join([sys.executable, str(script), str(child_file)])
        self.session.run_command(command.encode())
        self.assert_prompt_recovered()
        self.session.run_command(b"fg")
        self.assert_prompt_recovered()


if __name__ == "__main__":
    TerminalRecoveryTests.binary = str(Path(sys.argv[1]).resolve())
    unittest.main(argv=[sys.argv[0]], verbosity=2)
