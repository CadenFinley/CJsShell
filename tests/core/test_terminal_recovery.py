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
import time
import unittest

from test_idle_hook_interactive import IdleHookSession, normalize_terminal_output


INPUT_ABORTED = b"\x1b]133;D\x1b\\"


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
        output_start = self.session.wait_for(b"editing-ok\r\n", start)
        self.session.wait_for_prompt(output_start, command_completed=True)

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

    def test_stdout_redirection_preserves_commands_and_terminal_editor(self) -> None:
        target = Path(self.directory.name) / "stdout"
        marker = Path(self.directory.name) / ".cache" / "cjsh" / ".first_boot"
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.touch()
        session = IdleHookSession(self.binary, self.directory.name, stdout_path=str(target),
                                  terminal_size=(24, 100))
        self.addCleanup(session.close)
        session.wait_for_prompt(0)
        start = len(session.output)
        session.write(b"printf 'COMMAND-ONLY\\n'\r")
        session.wait_for_prompt(start, command_completed=True)
        self.assertEqual(target.read_bytes().strip(), b"COMMAND-ONLY")
        self.assertIn(b"printf", normalize_terminal_output(bytes(session.output)))

    def test_external_sigint_discards_partial_command_and_recovers(self) -> None:
        marker = Path(self.directory.name) / "must-not-run"
        start = len(self.session.output)
        command = ("touch " + shlex.quote(str(marker))).encode()
        # Wait until the whole unsubmitted command reaches the editor before
        # interrupting it; queued typing can otherwise arrive after SIGINT.
        self.session.write(b"\x1b[200~" + command + b"\x1b[201~")
        self.session.wait_for(marker.name.encode(), start)
        start = len(self.session.output)
        os.kill(self.session.pid, signal.SIGINT)
        # Redraws also emit prompt markers. Wait for cancellation, then for the
        # empty command to finish, before checking that the old line was discarded.
        start = self.session.wait_for(INPUT_ABORTED, start)
        self.session.wait_for_prompt(start)
        start = len(self.session.output)
        self.session.write(b"\r")
        self.session.wait_for_prompt(start, command_completed=True)
        self.assertFalse(marker.exists())
        self.assert_prompt_recovered()

    def test_external_signals_unwind_history_menu(self) -> None:
        for signum in (signal.SIGINT, signal.SIGHUP):
            with self.subTest(signum=signum):
                session = IdleHookSession(self.binary, self.directory.name, editor_args=[
                    "--no-prompt-vars", "--no-completions", "--no-syntax-highlighting",
                ])
                self.addCleanup(session.close)
                session.wait_for_prompt(0)
                session.run_command(b"printf 'history-seed\\n'")
                marker = Path(self.directory.name) / f"menu-must-not-run-{signum}"
                start = len(session.output)
                session.write(("touch " + shlex.quote(str(marker))).encode() + b"\x12")
                session.wait_for(b"history search:", start)
                start = len(session.output)
                os.kill(session.pid, signum)
                if signum == signal.SIGHUP:
                    self.assertEqual(session.wait_for_exit(), -signum)
                    self.assertTrue(termios.tcgetattr(session.fd)[3] & termios.ICANON)
                else:
                    start = session.wait_for(INPUT_ABORTED, start)
                    session.wait_for_prompt(start)
                    start = session.run_command(b"printf 'menu-editing-ok\\n'")
                    output = normalize_terminal_output(bytes(session.output[start:]))
                    self.assertIn(b"menu-editing-ok\n", output)
                self.assertFalse(marker.exists())

    def test_reset_ignored_sigint_restores_editor_interrupt(self) -> None:
        self.session.run_command(b"trap '' INT; trap - INT")
        self.test_external_sigint_discards_partial_command_and_recovers()

    def test_external_sighup_exits_with_terminal_still_open(self) -> None:
        os.kill(self.session.pid, signal.SIGHUP)
        self.assertEqual(self.session.wait_for_exit(), -signal.SIGHUP)
        self.assertTrue(termios.tcgetattr(self.session.fd)[3] & termios.ICANON)

    def test_external_sigint_runs_trap_before_next_prompt(self) -> None:
        self.session.run_command(b"trap 'printf \"signal-trap-ran\\n\"' INT")
        start = len(self.session.output)
        os.kill(self.session.pid, signal.SIGINT)
        self.session.wait_for(b"signal-trap-ran\r\n", start)
        self.session.wait_for_prompt(start)
        self.assert_prompt_recovered()

    def test_ignored_sigint_does_not_interrupt_editor(self) -> None:
        self.session.run_command(b"trap '' INT")
        start = len(self.session.output)
        self.session.write(b"printf 'ignored-%s\\n' ok")
        self.session.pump(0.1)
        os.kill(self.session.pid, signal.SIGINT)
        self.session.pump(0.1)
        self.session.write(b"\r")
        self.session.wait_for_prompt(start, command_completed=True)
        self.assertIn(b"\nignored-ok\n", normalize_terminal_output(bytes(self.session.output[start:])))

    def test_foreground_cat_receives_lf_immediately(self) -> None:
        target = Path(self.directory.name) / "cat-output"
        start = len(self.session.output)
        self.session.write(("/bin/cat > " + shlex.quote(str(target)) + "\r").encode())
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline and (not target.exists() or
                os.tcgetpgrp(self.session.fd) == self.session.pid):
            self.session.pump()
        self.assertTrue(target.exists())
        self.assertFalse(termios.tcgetattr(self.session.fd)[0] & termios.INLCR)
        self.session.write(b"hello\n")
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline and target.read_bytes() != b"hello\n":
            self.session.pump()
        self.assertEqual(target.read_bytes(), b"hello\n")
        self.session.write(b"\x04")
        self.session.wait_for_prompt(start, command_completed=True)
        self.assert_prompt_recovered()

    def test_stty_changes_persist_but_prompt_echo_recovers(self) -> None:
        probe = Path(self.directory.name) / "tty_settings.py"
        target = Path(self.directory.name) / "tty-settings.json"
        probe.write_text("""
import json, sys, termios
a = termios.tcgetattr(0)
with open(sys.argv[1], 'w') as output:
    json.dump([bool(a[3] & termios.ECHO), bool(a[3] & termios.TOSTOP),
               a[6][termios.VINTR][0], a[6][termios.VERASE][0]], output)
""", encoding="utf-8")
        command = shlex.join([sys.executable, str(probe), str(target)])
        for monitor in (b"set -m", b"set +m"):
            with self.subTest(monitor=monitor):
                self.session.run_command(monitor)
                self.session.run_command(("stty -echo intr '^G' erase '^H' tostop; " + command).encode())
                self.assertEqual(json.loads(target.read_text()), [False, True, 7, 8])
                self.session.run_command(command.encode())
                self.assertEqual(json.loads(target.read_text()), [True, True, 7, 8])
                start = len(self.session.output)
                self.session.write(b"visible-echo-probe")
                # Per-character redraws can outlast a fixed delay on busy CI runners.
                self.session.wait_for(b"visible-echo-probe", start)
                self.session.write(b"\x15")
                self.session.run_command(b"stty echo intr '^C' erase '^?' -tostop")

    def test_custom_fd_redirections_preserve_foreground_launches(self) -> None:
        file_list = Path(self.directory.name) / "files"
        file_list.write_text("sample\n", encoding="utf-8")
        probe = Path(self.directory.name) / "foreground_probe.py"
        probe.write_text("""
import os
import termios
fd = os.open('/dev/tty', os.O_RDWR)
assert os.tcgetpgrp(fd) == os.getpgrp(), 'job launched in the background'
termios.tcsetattr(fd, termios.TCSANOW, termios.tcgetattr(fd))
os.close(fd)
print('foreground-access-ok', flush=True)
""", encoding="utf-8")
        probe_command = shlex.join([sys.executable, str(probe)])

        def check_foreground(command: str) -> None:
            start = self.session.run_command(command.encode())
            output = normalize_terminal_output(bytes(self.session.output[start:]))
            self.assertIn(b"\nforeground-access-ok\n", output)
            self.assertNotIn(b"Stopped", output)
            self.assert_prompt_recovered()

        check_foreground(probe_command)
        for fd in (3, 4, 5, 6, 7, 8, 9):
            with self.subTest(fd=fd):
                # cjcloc reads its list through fd 3. Exercise the same loop with
                # low descriptors that can otherwise collide with shell internals.
                command = (
                    f'while IFS= read -r file <&{fd}; do '
                    f'printf "%s\\n" "$file"; {probe_command}; '
                    f'done {fd}<{shlex.quote(str(file_list))}'
                )
                check_foreground(command)
                check_foreground(probe_command)

    def test_builtin_fd_redirection_does_not_break_next_prompt(self) -> None:
        self.session.run_command(b"true 3</dev/null")
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
    # Check the modes needed by prompt hooks. Other stty changes now persist.
    a = termios.tcgetattr(0)
    json.dump([os.tcgetpgrp(0), a[0] & (termios.ICRNL | termios.INLCR),
               a[1] & (termios.OPOST | termios.ONLCR),
               a[3] & (termios.ECHO | termios.ICANON | termios.IEXTEN | termios.ISIG)], output)
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
