#!/usr/bin/env python3

# test_syntax_regressions.py
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

"""Compare editor validation, syntax-only mode, and execution across syntax layouts.

Fixtures carry expected output and status so tests do not depend on another shell.
"""

import json
from pathlib import Path
import subprocess
import sys


def main():
    binary, validator = sys.argv[1:3]
    cases = json.loads(Path(__file__).with_name("syntax_regression_cases.json").read_text())["cases"]
    failures = 0
    for case in cases:
        problems = []
        try:
            validation = subprocess.run(
                [validator, case["script"]], capture_output=True, text=True, timeout=5,
                check=True,
            ).stdout
            more = "MORE 1" in validation
            syntax_errors = [line for line in validation.splitlines()
                             if line.startswith(("ERROR SYN", "ERROR FUNC", "ERROR POSIX"))]
            if more == case["complete"]:
                problems.append(f"continuation requested: {more}")
            if case["complete"] and syntax_errors:
                problems.extend(syntax_errors)
            mode = "-c" if case["complete"] else "-nc"
            actual = subprocess.run(
                [binary, "--no-config", mode, case["script"]],
                capture_output=True, text=True, timeout=5,
            )
            if case["complete"]:
                if actual.returncode != case["status"] or actual.stdout != case["stdout"]:
                    problems.append(
                        f"expected status={case['status']}, stdout={case['stdout']!r}; "
                        f"got status={actual.returncode}, stdout={actual.stdout!r}, "
                        f"stderr={actual.stderr!r}")
            elif actual.returncode == 0:
                problems.append("syntax-only mode accepted incomplete input")
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
            problems.append(str(error))
        if problems:
            failures += 1
            print(f"FAIL: {case['name']}: " + " | ".join(problems))
    print(f"Total tests: {len(cases)}")
    print(f"Passed: {len(cases) - failures}")
    print(f"Failed: {failures}")
    if failures:
        print(f"{failures}/{len(cases)} syntax regression tests failed")
    else:
        print(f"All {len(cases)} syntax regression tests passed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
