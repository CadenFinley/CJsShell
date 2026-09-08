#!/usr/bin/env python3

# test_batch2_startup.py
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

"""Batch 2 invocation, startup paths, environment and persistence contracts."""
from pathlib import Path
import os
import pwd
import shlex
import subprocess
import sys
import tempfile
import unittest

from test_idle_hook_interactive import IdleHookSession


SKIP_PRELOAD_INJECTION = os.environ.get("CJSH_TEST_SKIP_PRELOAD_INJECTION") == "1"


class StartupTests(unittest.TestCase):
    binary: str
    injector: str

    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="cjsh-batch2-")
        self.addCleanup(directory.cleanup)
        self.home = Path(directory.name).resolve()
        self.env = dict(os.environ, HOME=str(self.home), TERM="xterm")
        for name in ("CJSH_ENV", "CJSH_CONFIG_HOME", "CJSH_HISTORY_FILE", "ENV"):
            self.env.pop(name, None)

    def run_shell(self, *args, input=None):
        return subprocess.run([self.binary, *args], input=input, env=self.env,
                              text=True, capture_output=True, timeout=8)

    def trace_files(self, root, prefix=""):
        root.mkdir(parents=True, exist_ok=True)
        for name, stage in ((".cjshenv", "env"), (".cjprofile", "profile"),
                            (".cjshrc", "rc"), (".cjlogout", "logout")):
            (root / name).write_text(f'echo {prefix}{stage} >> "$HOME/trace"\n')

    def read_trace(self):
        path = self.home / "trace"
        result = path.read_text().splitlines() if path.exists() else []
        path.unlink(missing_ok=True)
        return result

    def test_invocation_streams(self):
        for args in (("--unknown-batch2",), ("-Z",), ("-c",), ("--command",),
                     ("--config-dir",), ("--config-dir=",), ("--login-path",)):
            with self.subTest(args=args):
                r = self.run_shell(*args)
                self.assertEqual(r.returncode, 1)
                self.assertEqual(r.stdout, "")
                self.assertIn("Usage:", r.stderr)
        for arg in ("--help", "--version"):
            r = self.run_shell(arg)
            self.assertEqual(r.returncode, 0)
            self.assertTrue(r.stdout)
            self.assertEqual(r.stderr, "")
        help_text = self.run_shell("--help").stdout
        self.assertIn("--no-system-paths", help_text)
        self.assertNotIn("--login-path", help_text)

    def test_viewport_limit_commands(self):
        for command, label, default in (("menu-max-lines", "Menu content", 50),
                                        ("multiline-max-lines", "Multiline input", 15)):
            result = self.run_shell("-c", f"cjshopt {command} status")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, f"{label} currently shows up to {default} lines.\n")
            for requested, applied in ((1, 1), (8, 8), (75, 75), (300, 256)):
                with self.subTest(command=command, requested=requested):
                    result = self.run_shell("-c", f"cjshopt {command} {requested}; "
                                            f"cjshopt {command} --status")
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stderr, "")
                    unit = "line" if applied == 1 else "lines"
                    self.assertEqual(result.stdout.splitlines()[-1],
                                     f"{label} currently shows up to {applied} {unit}.")
            for value in ("", "0", "-1", "+2", "1.5", "invalid", "8 extra",
                          "999999999999999999999999999"):
                with self.subTest(command=command, invalid=value):
                    result = self.run_shell("-c", f"cjshopt {command} {value}")
                    self.assertEqual(result.returncode, 1)
                    self.assertIn(command, result.stderr)

        result = self.run_shell("-c", "cjshopt menu-max-lines 8; "
                                "cjshopt multiline-max-lines status")
        self.assertEqual(result.stderr, "")
        self.assertEqual(result.stdout.splitlines()[-1],
                         "Multiline input currently shows up to 15 lines.")
        result = self.run_shell("-c", "cjshopt menu-max-lines --help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("menu-max-lines <count|status>", result.stdout)
        self.assertIn("menu-max-lines", self.run_shell("-c", "cjshopt --help").stdout)

    def test_menu_limit_from_rc_is_quiet(self):
        (self.home / ".cjshrc").write_text(
            "cjshopt menu-max-lines 8\ncjshopt menu-max-lines status\n")
        result = self.run_shell("-i", "--no-titleline", "--no-history", "-c",
                                "cjshopt menu-max-lines status")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "Menu content currently shows up to 8 lines.\n")

    def test_noexec_sources(self):
        self.trace_files(self.home)
        marker = self.home / "executed"
        for flag in ("-n", "--no-exec"):
            for body, valid in ((f'touch {shlex.quote(str(marker))}\n', True),
                                ("if true; then\necho incomplete\n", False),
                                ("echo 'unterminated\n", False)):
                for source in ("command", "script", "stdin"):
                    with self.subTest(flag=flag, source=source, valid=valid):
                        args = [flag, "-l"]
                        if source == "command":
                            args += ["-c", body]
                        elif source == "script":
                            path = self.home / "script"
                            path.write_text(body)
                            args.append(str(path))
                        r = self.run_shell(*args, input=body if source == "stdin" else None)
                        self.assertEqual(r.returncode == 0, valid, r.stderr)
                        self.assertEqual(r.stdout, "")
                        self.assertFalse(marker.exists())
                        self.assertEqual(self.read_trace(), [])
        for flag in ("-m", "-s"):
            self.assertEqual(self.run_shell(flag, "-c", "echo executed").stdout, "executed\n")

    def test_native_configuration_precedence_and_bypass(self):
        alt = self.home / ".config/cjsh"
        override = self.home / "override root"
        cli = self.home / "cli root"
        self.trace_files(alt, "alt-")
        self.trace_files(override, "override-")
        self.trace_files(cli, "cli-")
        args = ("-il", "--no-titleline", "--no-history", "-c", 'echo body >> "$HOME/trace"')
        self.assertEqual(self.run_shell(*args).returncode, 0)
        self.assertEqual(self.read_trace(), ["alt-env", "alt-profile", "alt-rc", "body", "alt-logout"])
        self.trace_files(self.home)
        self.run_shell(*args)
        self.assertEqual(self.read_trace(), ["env", "profile", "rc", "body", "logout"])
        self.env["CJSH_CONFIG_HOME"] = str(override)
        self.run_shell(*args)
        self.assertEqual(self.read_trace(), ["override-env", "override-profile", "override-rc", "body", "override-logout"])
        self.run_shell("--config-dir", str(cli), *args)
        self.assertEqual(self.read_trace(), ["cli-env", "cli-profile", "cli-rc", "body", "cli-logout"])
        self.env["CJSH_ENV"] = str(self.home / ".cjshenv")
        self.run_shell(*args)
        self.assertEqual(self.read_trace(), ["env", "override-profile", "override-rc", "body", "override-logout"])
        self.env["CJSH_ENV"] = str(self.home / "missing")
        self.run_shell(*args)
        self.assertEqual(self.read_trace(), ["override-profile", "override-rc", "body", "override-logout"])
        for bypass in ("--no-config", "--secure"):
            self.run_shell(bypass, *args)
            self.assertEqual(self.read_trace(), ["body"])
        self.env.pop("CJSH_ENV")
        self.run_shell("--no-source", *args)
        self.assertEqual(self.read_trace(), ["override-env", "override-profile", "body", "override-logout"])
        self.run_shell("--config-dir", str(self.home / "missing-root"), *args)
        self.assertEqual(self.read_trace(), ["body"])
        self.assertFalse((self.home / "missing-root").exists())

    def test_posix_startup_and_env_expansion(self):
        self.trace_files(self.home, "native-")
        (self.home / ".profile").write_text('echo profile >> "$HOME/trace"\n')
        (self.home / "interactive env").write_text('echo ENV >> "$HOME/trace"\n')
        self.env["ENV"] = '${HOME}/interactive env'
        system_profile = self.home / "system-profile"
        system_profile.write_text('echo system >> "$HOME/trace"\n')
        self.env["CJSH_TEST_SYSTEM_PROFILE"] = str(system_profile)
        self.env["DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"] = self.injector
        sh = self.home / "sh"
        sh.symlink_to(self.binary)
        for binary in (self.binary, str(sh)):
            for login in (False, True):
                for interactive in (False, True):
                    with self.subTest(binary=binary, login=login, interactive=interactive):
                        if login and SKIP_PRELOAD_INJECTION:
                            self.skipTest("system-profile injection requires a dynamic binary")
                        args = [binary, "--posix", "--no-sh-warning", "--no-history"]
                        args += (["-l"] if login else []) + (["-i"] if interactive else [])
                        args += ["-c", 'echo body >> "$HOME/trace"']
                        r = subprocess.run(args, env=self.env, capture_output=True, text=True, timeout=8)
                        self.assertEqual(r.returncode, 0, r.stderr)
                        self.assertEqual(self.read_trace(), (["system", "profile"] if login else []) +
                                         (["ENV"] if interactive else []) + ["body"])
        self.run_shell("--posix", "-il", "--no-config", "-c", ":")
        self.assertEqual(self.read_trace(), [])
        self.env["ENV"] = '$(touch "$HOME/unwanted")'
        self.run_shell("--posix", "-i", "-c", ":")
        self.assertFalse((self.home / "unwanted").exists())

    def test_failed_exec_contract(self):
        for mode, expected in (([], 0), (["--posix"], 127), (["--posix", "-i"], 0)):
            r = self.run_shell("--no-config", *mode, "-c", "exec /missing-batch2-command; echo survived")
            self.assertEqual(r.returncode, expected, r.stderr)
            self.assertEqual(r.stdout, "" if expected else "survived\n")
        path = self.home / "nonexecutable"
        path.write_text("echo no\n")
        r = self.run_shell("--posix", "-c", f"exec {shlex.quote(str(path))}; echo survived")
        self.assertEqual(r.returncode, 126)
        self.assertEqual(r.stdout, "")

    def child_environment(self, *args):
        r = self.run_shell(*args, "-c", "/usr/bin/env")
        self.assertEqual(r.returncode, 0, r.stderr)
        return dict(line.split("=", 1) for line in r.stdout.splitlines() if "=" in line)

    def test_environment_supplied_empty_absent(self):
        names = ("USER", "LOGNAME", "LANG", "PAGER", "TMPDIR", "PATH", "MANPATH")
        for value in ("batch2-inherited", "", None):
            for name in names:
                if value is None:
                    self.env.pop(name, None)
                else:
                    self.env[name] = value
            for args in (["--no-system-paths"], ["-l", "--no-system-paths"],
                         ["--no-config"], ["-l", "--no-config"],
                         ["-l", "--secure"], ["-l", "-m"], ["--posix"]):
                with self.subTest(value=value, args=args):
                    child = self.child_environment(*args)
                    for name in names:
                        expected = pwd.getpwuid(os.getuid()).pw_name if value is None and name in ("USER", "LOGNAME") else value
                        self.assertEqual(child.get(name), expected, name)
        # An internal exec search fallback must not export a synthesized PATH.
        r = self.run_shell("--no-config", "-c", "env")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(any(line.startswith("PATH=") for line in r.stdout.splitlines()))

    def test_default_system_paths(self):
        for value in ("/batch2/supplied:/batch2/supplied/bin:/batch2/supplied", "", None):
            for manpath in ("/batch2/man", "", None):
                for name, entry in (("PATH", value), ("MANPATH", manpath)):
                    if entry is None:
                        self.env.pop(name, None)
                    else:
                        self.env[name] = entry
                for args in ([], ["-l"], ["-i", "--no-titleline", "--no-history"],
                             ["-il", "--no-titleline", "--no-history"]):
                    with self.subTest(value=value, manpath=manpath, args=args):
                        child = self.child_environment(*args)
                        self.assertTrue(child.get("PATH"))
                        parts = child["PATH"].split(":")
                        if value and "-l" not in args and "-il" not in args:
                            self.assertEqual(child["PATH"], value)
                        else:
                            self.assertEqual(len(parts), len(set(parts)))
                        if value:
                            self.assertIn("/batch2/supplied", parts)
                            self.assertIn("/batch2/supplied/bin", parts)
                        else:
                            # This was the terminal-startup regression: commands must
                            # be found and children must receive the initialized PATH.
                            r = self.run_shell(*args, "-c", "env")
                            self.assertEqual(r.returncode, 0, r.stderr)
                            self.assertIn("PATH=" + child["PATH"], r.stdout.splitlines())
                        self.assertEqual(child.get("MANPATH"), manpath)

    def test_nonlogin_preserves_path_components(self):
        for value in (":/custom/bin::/usr/bin:/custom/bin:", ":", "::", " "):
            self.env["PATH"] = value
            for args in ([], ["-i", "--no-titleline", "--no-history"]):
                with self.subTest(value=value, args=args):
                    self.assertEqual(self.child_environment(*args)["PATH"], value)

    def test_nested_shell_preserves_toolchain_precedence(self):
        toolchain = self.home / "toolchain/bin"
        toolchain.mkdir(parents=True)
        executable = toolchain / "ls"
        executable.write_text("#!/bin/sh\nprintf 'toolchain-ls\\n'\n")
        executable.chmod(0o755)
        self.env["PATH"] = "/usr/bin:/bin"
        inherited = f"{toolchain}:/usr/bin:/bin::{toolchain}:"
        command = 'printf "%s\\n" "$PATH" "$(command -v ls)" "$(ls)"'
        for args in ([], ["-i", "--no-titleline", "--no-history"]):
            with self.subTest(args=args):
                nested = shlex.join([self.binary, *args, "-c", command])
                r = self.run_shell("-c", f"PATH={shlex.quote(inherited)}; {nested}")
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertEqual(r.stdout.splitlines(), [inherited, str(executable), "toolchain-ls"])

    def test_system_paths_precede_native_configuration(self):
        (self.home / ".cjshenv").write_text('PATH="/batch2/env:$PATH"\n')
        (self.home / ".cjprofile").write_text('PATH="/batch2/profile:$PATH"\n')
        self.env.pop("PATH", None)
        child = self.child_environment("-l")
        self.assertTrue(child["PATH"].startswith("/batch2/profile:/batch2/env:"))
        self.assertIn("/usr/bin", child["PATH"].split(":"))
        self.env["PATH"] = "/batch2/inherited"
        child = self.child_environment("-l", "--no-system-paths")
        self.assertEqual(child["PATH"], "/batch2/profile:/batch2/env:/batch2/inherited")

    def test_system_paths_flag_is_invocation_only(self):
        r = self.run_shell("-c", "cjshopt login-startup-arg --no-system-paths")
        self.assertNotEqual(r.returncode, 0)

    def test_unavailable_persistence(self):
        missing = self.home / "missing"
        readonly = self.home / "readonly"
        readonly.mkdir()
        readonly.chmod(0o500)
        self.addCleanup(readonly.chmod, 0o700)
        for path in (missing, readonly):
            self.env["HOME"] = str(path)
            r = self.run_shell("-i", "--no-config", "-c", "echo usable")
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(r.stdout, "usable\n")
            if os.getuid() != 0:
                self.assertEqual(r.stderr.count("persistence unavailable"), 1, r.stderr)
        self.assertFalse(missing.exists())
        self.env["HOME"] = str(self.home)
        self.env["CJSH_HISTORY_FILE"] = str(self.home)  # directory, not a file
        r = self.run_shell("-i", "--no-config", "-c", "echo usable")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr.count("persistence unavailable"), 1)
        session = IdleHookSession(self.binary, str(missing), argv=[self.binary, "--no-config", "--no-titleline", "--no-prompt-vars"])
        self.addCleanup(session.close)
        session.wait_for_prompt(0)
        session.run_command(b"echo usable-one")
        session.run_command(b"echo usable-two")
        self.assertEqual(bytes(session.output).count(b"persistence unavailable"), 1)
        session.write(b"exit\r")
        self.assertEqual(session.wait_for_exit(), 0)

    def test_invalid_history_and_completion_storage_are_independent(self):
        fifo = self.home / "history-fifo"
        os.mkfifo(fifo)
        self.env["CJSH_HISTORY_FILE"] = str(fifo)
        r = self.run_shell("-i", "--no-config", "-c", "echo usable")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, "usable\n")
        self.assertEqual(r.stderr.count("persistence unavailable"), 1)
        self.env.pop("CJSH_HISTORY_FILE")
        completion = self.home / ".cache/cjsh/generated_completions"
        completion.rmdir()
        completion.write_text("not a directory")
        r = self.run_shell("-i", "--no-config", "-c", "echo usable")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr.count("persistence unavailable"), 1)
        self.assertIn("generated_completions", r.stderr)
        self.assertTrue((self.home / ".cache/cjsh/history.txt").is_file())

    @unittest.skipIf(SKIP_PRELOAD_INJECTION, "credential injection requires a dynamic binary")
    def test_privileged_startup_refused_before_files(self):
        self.trace_files(self.home)
        self.env["DYLD_INSERT_LIBRARIES" if sys.platform == "darwin" else "LD_PRELOAD"] = self.injector
        for kind in ("UID", "GID"):
            self.env[f"CJSH_TEST_{kind}_MISMATCH"] = "1"
            for args in ([], ["--posix"], ["--no-config"]):
                r = self.run_shell(*args, "-il", "-c", "echo executed")
                self.assertEqual(r.returncode, 1, r.stderr)
                self.assertEqual(r.stdout, "")
                self.assertIn("mismatched real and effective IDs", r.stderr)
                self.assertEqual(self.read_trace(), [])
            del self.env[f"CJSH_TEST_{kind}_MISMATCH"]


if __name__ == "__main__":
    StartupTests.binary = str(Path(sys.argv[1]).resolve())
    StartupTests.injector = str(Path(sys.argv[2]).resolve())
    unittest.main(argv=[sys.argv[0]], verbosity=2)
