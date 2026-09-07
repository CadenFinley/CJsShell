#!/usr/bin/env python3
"""Regressions for terminal reply ownership and signal dispositions.

MIT License. Copyright (c) 2026 Caden Finley.
"""
import os
from pathlib import Path
import signal
import sys
import tempfile
import termios
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "core"))
from test_idle_hook_interactive import IdleHookSession, normalize_terminal_output


class TerminalContractTests(unittest.TestCase):
    binary: str

    def start(self, scenario: str, *args: str) -> IdleHookSession:
        directory = tempfile.TemporaryDirectory(prefix="isocline-terminal-contract-")
        self.addCleanup(directory.cleanup)
        query = scenario in ("query", "osc")
        session = IdleHookSession(self.binary, directory.name,
                                  argv=[self.binary, scenario, *args],
                                  cursor_response=None if query else b"\x1b[1;1R",
                                  terminal_size=(24, 100))
        self.addCleanup(session.close)
        session.wait_for(b"QUERY_READY" if query else b"SIGNAL_READY")
        return session

    def test_rejected_replies_preserve_every_byte(self) -> None:
        for data in (b"ordinary", b"\x00", b"\x1b", b"\x1b[", b"\x1b[12;",
                     b"\x1b[1;2A", b"\x1b[0;1R", b"\x1b[1;0R",
                     b"\x1b[99999999999999999999999999999;1R",
                     b"\x1b[200~pasted\x1b[201~",
                     b"\x1b]0;title\x07", b"\x1b[" + b"1" * 260):
            with self.subTest(data=data):
                session = self.start("query", str(len(data)))
                session.wait_for(b"\x1b[6n")
                session.write(data)
                self.assertEqual(session.wait_for_exit(), 0)
                output = normalize_terminal_output(bytes(session.output))
                self.assertIn(b"QUERY:0:0:0\n", output)
                self.assertIn(data.hex().encode() + b"\nREPLAY_DONE", output)

    def test_matching_reply_consumes_only_the_reply(self) -> None:
        session = self.start("query", "4")
        session.wait_for(b"\x1b[6n")
        session.write(b"\x1b[12;34Rtail")
        self.assertEqual(session.wait_for_exit(), 0)
        output = normalize_terminal_output(bytes(session.output))
        self.assertIn(b"QUERY:1:12:34\n", output)
        self.assertIn(b"7461696c\nREPLAY_DONE", output)

    def test_osc_requires_a_complete_matching_reply(self) -> None:
        payload = b"\x1b]4;0;rgb:ff/ff/ff"
        for data in (b"ordinary", payload, payload + b"\x1b",
                     payload + b"\x00typed\x07", b"\x1b]4;1;rgb:ff/ff/ff\x07"):
            with self.subTest(data=data):
                session = self.start("osc", str(len(data)))
                session.wait_for(b"\x1b]4;0;?\x07")
                session.write(data)
                self.assertEqual(session.wait_for_exit(), 0)
                output = normalize_terminal_output(bytes(session.output))
                self.assertIn(b"QUERY:0:0:0\n", output)
                self.assertIn(data.hex().encode() + b"\nREPLAY_DONE", output)
        for ending in (b"\x07", b"\x1b\\"):
            with self.subTest(ending=ending):
                session = self.start("osc", "4")
                session.wait_for(b"\x1b]4;0;?\x07")
                session.write(payload + ending + b"tail")
                self.assertEqual(session.wait_for_exit(), 0)
                output = normalize_terminal_output(bytes(session.output))
                self.assertIn(b"QUERY:1:0:0\n", output)
                self.assertIn(b"7461696c\nREPLAY_DONE", output)

    def test_default_signal_actions(self) -> None:
        for signum in (signal.SIGINT, signal.SIGHUP, signal.SIGTERM):
            with self.subTest(signum=signum):
                session = self.start("default")
                os.kill(session.pid, signum)
                self.assertEqual(session.wait_for_exit(), -signum)
                self.assertTrue(termios.tcgetattr(session.fd)[3] & termios.ICANON)

    def test_ignored_and_custom_signals_resume_raw_input(self) -> None:
        for scenario in ("ignore", "custom", "siginfo"):
            for signum in (signal.SIGINT, signal.SIGHUP, signal.SIGTERM):
                with self.subTest(scenario=scenario, signum=signum):
                    session = self.start(scenario)
                    original = termios.tcgetattr(session.fd)
                    os.kill(session.pid, signum)
                    if scenario == "ignore":
                        session.pump(0.1)
                    else:
                        session.wait_for(b"HANDLED\r\n")
                    self.assertEqual(termios.tcgetattr(session.fd), original)
                    session.write(b"\r")
                    self.assertEqual(session.wait_for_exit(), 0)
                    output = normalize_terminal_output(bytes(session.output))
                    expected = b"HANDLER:0:0" if scenario == "ignore" else f"HANDLER:{signum}:1".encode()
                    self.assertIn(expected, output)
                    self.assertIn(b"RESTORED:1", output)

    def test_default_disposition_restored_on_teardown(self) -> None:
        session = self.start("default")
        session.write(b"\r")
        self.assertEqual(session.wait_for_exit(), 0)
        self.assertIn(b"RESTORED:1", session.output)

    def test_reset_hand_is_honored(self) -> None:
        session = self.start("reset")
        if b"UNREPORTED_RESETHAND" in session.output:
            self.assertEqual(session.wait_for_exit(), 77)
            self.skipTest("sigaction does not report SA_RESETHAND on this platform")
        os.kill(session.pid, signal.SIGINT)
        session.wait_for(b"HANDLED\r\n")
        os.kill(session.pid, signal.SIGINT)
        self.assertEqual(session.wait_for_exit(), -signal.SIGINT)


if __name__ == "__main__":
    TerminalContractTests.binary = str(Path(sys.argv[1]).resolve())
    unittest.main(argv=[sys.argv[0]], verbosity=2)
