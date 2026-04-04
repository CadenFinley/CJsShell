#!/usr/bin/env python3

# test_build_system.py
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
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Mapping


TIMESTAMP_PATTERN = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")


def parse_cmake_cache(cache_path: Path) -> dict[str, str]:
    cache_values: dict[str, str] = {}
    for raw_line in cache_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        if ":" not in line or "=" not in line:
            continue
        key_with_type, value = line.split("=", 1)
        key, _sep, _value_type = key_with_type.partition(":")
        cache_values[key] = value
    return cache_values


def parse_compile_arguments(entry: dict[str, object]) -> list[str]:
    command_args = entry.get("arguments")
    if isinstance(command_args, list):
        return [str(arg) for arg in command_args]

    command = entry.get("command")
    if isinstance(command, str):
        return shlex.split(command)

    raise AssertionError("compile_commands entry did not provide command arguments")


def load_cpp_compile_arguments(build_dir: Path) -> list[str]:
    compile_commands_path = build_dir / "compile_commands.json"
    if not compile_commands_path.exists():
        raise AssertionError(f"Missing compilation database: {compile_commands_path}")

    entries = json.loads(compile_commands_path.read_text(encoding="utf-8"))
    if not isinstance(entries, list) or not entries:
        raise AssertionError("compile_commands.json was empty")

    selected_entry: dict[str, object] | None = None
    fallback_entry: dict[str, object] | None = None

    for entry in entries:
        source_file = Path(str(entry.get("file", "")))
        if source_file.suffix not in {".cpp", ".cc", ".cxx"}:
            continue
        if fallback_entry is None:
            fallback_entry = entry
        source_parts = source_file.parts
        if (
            len(source_parts) >= 2
            and source_parts[-2] == "src"
            and source_parts[-1] == "builtin.cpp"
        ):
            selected_entry = entry
            break

    if selected_entry is None:
        selected_entry = fallback_entry
    if selected_entry is None:
        raise AssertionError(
            "No C++ compile command entries found in compile_commands.json"
        )

    return parse_compile_arguments(selected_entry)


def extract_definitions(args: list[str]) -> dict[str, str]:
    definitions: dict[str, str] = {}
    for arg in args:
        if not arg.startswith("-D"):
            continue
        body = arg[2:]
        if "=" in body:
            key, value = body.split("=", 1)
        else:
            key, value = body, "1"
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
            value = value[1:-1]
        definitions[key] = value
    return definitions


def configure_cmake(
    source_dir: Path,
    cmake_args: list[str],
    env_overrides: Mapping[str, str] | None = None,
) -> Path:
    build_dir = Path(tempfile.mkdtemp(prefix="cjsh-build-system-test-"))
    command = ["cmake", "-S", str(source_dir), "-B", str(build_dir), *cmake_args]
    env = os.environ.copy()
    if env_overrides:
        env.update(env_overrides)

    completed = subprocess.run(
        command,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(
            "CMake configure failed with non-zero exit status\n"
            f"Command: {' '.join(command)}\n"
            f"Output:\n{completed.stdout}"
        )
    return build_dir


class BuildSystemConfigurationTests(unittest.TestCase):
    source_dir: Path = Path()

    @classmethod
    def setUpClass(cls) -> None:
        if shutil.which("cmake") is None:
            raise unittest.SkipTest("cmake is not available in PATH")
        if not (cls.source_dir / "CMakeLists.txt").exists():
            raise unittest.SkipTest(
                f"CMakeLists.txt was not found under {cls.source_dir}"
            )

    def configure_project(
        self,
        cmake_args: list[str],
        env_overrides: Mapping[str, str] | None = None,
    ) -> Path:
        build_dir = configure_cmake(self.source_dir, cmake_args, env_overrides)
        self.addCleanup(shutil.rmtree, build_dir, True)
        return build_dir

    def test_metadata_injection_uses_hash_override(self) -> None:
        forced_hash = "meta-test-hash-123"
        build_dir = self.configure_project(
            ["-DCMAKE_BUILD_TYPE=Release"],
            env_overrides={"CJSH_GIT_HASH_OVERRIDE": forced_hash},
        )
        args = load_cpp_compile_arguments(build_dir)
        definitions = extract_definitions(args)

        for key in (
            "CJSH_GIT_HASH",
            "CJSH_GIT_HASH_FULL",
            "CJSH_BUILD_ARCH",
            "CJSH_BUILD_PLATFORM",
            "CJSH_BUILD_TIME",
            "CJSH_BUILD_COMPILER",
            "CJSH_CXX_STANDARD",
            "CJSH_BUILD_TYPE",
        ):
            self.assertIn(key, definitions, f"Missing build metadata definition: {key}")

        self.assertEqual(definitions["CJSH_GIT_HASH"], forced_hash)
        self.assertEqual(definitions["CJSH_GIT_HASH_FULL"], forced_hash)
        self.assertEqual(definitions["CJSH_BUILD_TYPE"], "Release")
        self.assertRegex(definitions["CJSH_BUILD_TIME"], TIMESTAMP_PATTERN)

    def test_edge_case_default_build_type_and_strip_env_override(self) -> None:
        build_dir = self.configure_project([], env_overrides={"CJSH_STRIP_BINARY": "0"})
        cache = parse_cmake_cache(build_dir / "CMakeCache.txt")

        self.assertEqual(cache.get("CJSH_STRIP_BINARY"), "OFF")

        configuration_types = cache.get("CMAKE_CONFIGURATION_TYPES", "")
        if not configuration_types:
            self.assertEqual(cache.get("CMAKE_BUILD_TYPE"), "Release")
            args = load_cpp_compile_arguments(build_dir)
            self.assertIn("-DNDEBUG", args)

    def test_edge_case_cjsh_build_tests_forces_build_testing(self) -> None:
        build_dir = self.configure_project(
            [
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCJSH_BUILD_TESTS=ON",
                "-DBUILD_TESTING=OFF",
            ],
        )
        cache = parse_cmake_cache(build_dir / "CMakeCache.txt")

        self.assertEqual(cache.get("CJSH_BUILD_TESTS"), "ON")
        self.assertEqual(cache.get("BUILD_TESTING"), "ON")

    def test_release_preset_settings(self) -> None:
        build_dir = self.configure_project(["-DCMAKE_BUILD_TYPE=Release"])
        args = load_cpp_compile_arguments(build_dir)
        definitions = extract_definitions(args)

        self.assertIn("-O2", args)
        self.assertIn("-DNDEBUG", args)
        self.assertNotIn("-O0", args)
        self.assertNotIn("-Oz", args)
        self.assertIn("IC_NO_DEBUG_MSG", definitions)
        self.assertNotIn("DEBUG", definitions)
        self.assertNotIn("CJSH_MINIMAL_BUILD", definitions)

    def test_debug_preset_settings(self) -> None:
        build_dir = self.configure_project(["-DCMAKE_BUILD_TYPE=Debug"])
        args = load_cpp_compile_arguments(build_dir)
        definitions = extract_definitions(args)

        self.assertIn("-O0", args)
        self.assertIn("-g", args)
        self.assertIn("-fsanitize=address", args)
        self.assertNotIn("-O2", args)
        self.assertIn("DEBUG", definitions)
        self.assertNotIn("IC_NO_DEBUG_MSG", definitions)
        self.assertNotIn("NDEBUG", definitions)

    def test_minimal_preset_settings(self) -> None:
        build_dir = self.configure_project(
            ["-DCMAKE_BUILD_TYPE=Release", "-DCJSH_MINIMAL_BUILD=ON"],
        )
        args = load_cpp_compile_arguments(build_dir)
        definitions = extract_definitions(args)

        self.assertIn("-Oz", args)
        self.assertNotIn("-O2", args)
        self.assertIn("-DNDEBUG", args)
        self.assertEqual(definitions.get("CJSH_MINIMAL_BUILD"), "1")
        self.assertEqual(definitions.get("CJSH_NO_FANCY_FEATURES"), "1")
        self.assertEqual(definitions.get("IC_NO_DEBUG_MSG"), "1")


def resolve_source_dir(argv: list[str]) -> Path:
    if len(argv) >= 2:
        return Path(argv[1]).resolve()
    return Path(__file__).resolve().parents[2]


def main(argv: list[str]) -> int:
    source_dir = resolve_source_dir(argv)
    BuildSystemConfigurationTests.source_dir = source_dir

    test_suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        BuildSystemConfigurationTests
    )
    result = unittest.TextTestRunner(verbosity=2).run(test_suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
