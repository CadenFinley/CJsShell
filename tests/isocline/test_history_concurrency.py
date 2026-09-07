#!/usr/bin/env python3

# test_history_concurrency.py
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

"""Concurrent history transactions and interrupted writers preserve complete records."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest


class HistoryConcurrencyTests(unittest.TestCase):
    driver: str

    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="cjsh-history-race-")
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / "history"

    def start(self, worker, count, maximum=5000, mode="unique", path=None):
        p = subprocess.Popen([self.driver, str(path or self.path), str(maximum), str(worker), str(count), mode],
                             stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        def cleanup():
            if p.poll() is None:
                p.kill()
            p.wait()
        self.addCleanup(cleanup)
        return p

    def records(self):
        lines = self.path.read_text().splitlines()
        self.assertEqual(len(lines) % 2, 0, "partial history record")
        for i in range(0, len(lines), 2):
            self.assertTrue(lines[i].startswith("#"), lines[i])
            self.assertFalse(lines[i+1].startswith("#"), lines[i+1])
        return [(lines[i], lines[i+1]) for i in range(0, len(lines), 2)]

    def run_workers(self, count=60, maximum=5000, mode="unique"):
        workers = [self.start(i, count, maximum, mode) for i in range(6)]
        for p in workers:
            self.assertEqual(p.wait(timeout=20), 0)

    def test_concurrent_appends_and_startup_loading(self):
        self.run_workers()
        rows = self.records()
        self.assertEqual({command for _, command in rows}, {f"{w}-{i}" for w in range(6) for i in range(60)})
        for header, command in rows:
            self.assertIn("worker=" + command.split("-")[0], header)
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)

    def test_symlink_aliases_share_the_transaction_lock(self):
        self.path.touch()
        alias = self.path.with_name("alias")
        alias.symlink_to(self.path)
        workers = [self.start(i, 60, path=alias if i % 2 else self.path) for i in range(6)]
        for p in workers:
            self.assertEqual(p.wait(timeout=20), 0)
        self.assertTrue(alias.is_symlink())
        self.assertEqual({command for _, command in self.records()},
                         {f"{w}-{i}" for w in range(6) for i in range(60)})

    def test_duplicate_frequency_is_not_lost(self):
        self.run_workers(mode="shared")
        rows = self.records()
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][1], "shared")
        self.assertIn("frequency=360", rows[0][0])

    def test_retention_and_concurrent_readers(self):
        workers = [self.start(i, 100, maximum=37) for i in range(6)]
        while any(p.poll() is None for p in workers):
            if self.path.exists():
                self.assertLessEqual(len(self.records()), 37)
            time.sleep(.002)
        for p in workers:
            self.assertEqual(p.wait(timeout=20), 0)
        self.assertEqual(len(self.records()), 37)

    def test_interrupted_writer_keeps_committed_records(self):
        self.assertEqual(self.start("seed", 20).wait(timeout=10), 0)
        p = self.start("interrupted", 10000)
        time.sleep(.03)
        if p.poll() is None:
            os.kill(p.pid, signal.SIGKILL)
        p.wait(timeout=10)
        self.assertEqual(self.start("after", 1).wait(timeout=10), 0)
        commands = {command for _, command in self.records()}
        self.assertTrue({f"seed-{i}" for i in range(20)} <= commands)
        self.assertIn("after-0", commands)


if __name__ == "__main__":
    HistoryConcurrencyTests.driver = str(Path(sys.argv[1]).resolve())
    unittest.main(argv=[sys.argv[0]], verbosity=2)
