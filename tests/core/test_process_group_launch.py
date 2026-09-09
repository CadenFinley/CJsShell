#!/usr/bin/env python3

# test_process_group_launch.py
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

import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import tempfile
import unittest


@unittest.skipIf(os.environ.get("CJSH_TEST_SKIP_PRELOAD_INJECTION") == "1",
                 "preload injection is disabled for this build")
class ProcessGroupLaunchTests(unittest.TestCase):
    binary: str
    injector: str

    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory(prefix="cjsh-process-group-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.marker = self.root / "launched"
        self.injection = self.root / "injected"
        self.output = self.root / "output"
        probe = self.root / "probe.py"
        probe.write_text("""
import os
from pathlib import Path
import sys
Path(sys.argv[1]).write_text('launched')
assert os.getpgrp() != os.getpgid(os.getppid()), 'child kept the shell process group'
print('launched', flush=True)
""", encoding="utf-8")
        self.probe_command = shlex.join([sys.executable, str(probe), str(self.marker)])

    def run_case(self, command: str, mode: str) -> subprocess.CompletedProcess[str]:
        self.marker.unlink(missing_ok=True)
        self.injection.unlink(missing_ok=True)
        env = os.environ.copy()
        env["HOME"] = str(self.root)
        env["XDG_CONFIG_HOME"] = str(self.root / ".config")
        preload = "DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"
        env[preload] = self.injector
        env["CJSH_TEST_SETPGID_MODE"] = mode
        env["CJSH_TEST_SETPGID_RESULT_FILE"] = str(self.injection)
        process = subprocess.Popen(
            [self.binary, "--no-source", "--minimal", "-c", "set -m\n" + command],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=env, text=True, start_new_session=True,
        )
        try:
            stdout, stderr = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
            self.fail("process-group launch did not complete")
        self.assertTrue(self.injection.exists(), f"setpgid injection did not run: {stderr}")
        return subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr)

    def launch_commands(self) -> dict[str, str]:
        return {
            "foreground": self.probe_command,
            "redirected": self.probe_command + " > " + shlex.quote(str(self.output)),
            "background": self.probe_command + ' &\npid=$!\nwait "$pid"',
            "pipeline": "printf input | " + self.probe_command,
        }

    def test_already_grouped_children_execute(self) -> None:
        for name, command in self.launch_commands().items():
            with self.subTest(launch=name):
                result = self.run_case(command, "already-grouped")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(self.marker.exists(), result.stderr)
                output = self.output.read_text() if name == "redirected" else result.stdout
                self.assertIn("launched", output)
                self.assertNotIn("runtime error", result.stderr)

    def test_ungrouped_children_still_fail(self) -> None:
        for name, command in self.launch_commands().items():
            with self.subTest(launch=name):
                result = self.run_case(command, "denied")
                self.assertFalse(self.marker.exists(), "command ran without its job process group")
                self.assertIn("failed to set process group", result.stderr)
                self.assertIn("Operation not permitted", result.stderr)

    def test_file_scanning_function_preserves_counts(self) -> None:
        sample = self.root / "sample"
        sample.write_text("one\ntwo\n", encoding="utf-8")
        files = self.root / "files"
        files.write_text((str(sample) + "\n") * 3, encoding="utf-8")
        command = """
scan() {
    counted=0
    total=0
    while IFS= read -r file <&3; do
        if LC_ALL=C grep -Iq . "$file" 2>/dev/null; then
            loc=$(wc -l <"$file")
            total=$((total + loc))
            counted=$((counted + 1))
        fi
    done 3<"$1"
    printf 'files=%d lines=%d\\n' "$counted" "$total"
}
scan """ + shlex.quote(str(files))
        result = self.run_case(command, "already-grouped")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "files=3 lines=6")
        self.assertNotIn("runtime error", result.stderr)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("Usage: test_process_group_launch.py <cjsh-binary> <injector-library>")
    ProcessGroupLaunchTests.binary = str(Path(sys.argv.pop(1)).resolve())
    ProcessGroupLaunchTests.injector = str(Path(sys.argv.pop(1)).resolve())
    unittest.main()
