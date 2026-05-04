<!--
  CONTRIBUTING.md

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
-->

# Contributing to CJ's Shell

Thanks for contributing to `cjsh`. The most useful pull requests are focused, tested locally, and accompanied by the documentation updates needed to explain behavior changes.

## Before You Start

- Search existing issues and pull requests before starting duplicate work.
- Open an issue before large behavioral changes, wide refactors, or compatibility-impacting work.
- Keep feature work, refactors, and formatting-only changes in separate pull requests when possible.
- Base pull requests on `master`.

## Tooling

You will need:

- C compiler
- C++ compiler
- CMake 3.25 or newer
- Ninja
- Python 3 for parts of the test suite
- `clang-format` 15 or newer for formatting C and C++ sources

## Build

From the repository root:

```bash
git clone https://github.com/CadenFinley/CJsShell && cd CJsShell
cmake --preset release
cmake --build --preset release --parallel
```

Useful presets:

- `release`: optimized default build
- `debug`: debug build with AddressSanitizer enabled
- `minimal`: size-focused release profile
- `relwithdebinfo`: optimized build with symbols
- `minsizerel`: CMake `MinSizeRel` profile

List presets with `cmake --list-presets`.

## Local Verification

Before opening a pull request, run the checks that match the scope of your change. For most code changes, that means:

```bash
ctest --preset release
./tests/run_shell_tests.sh "build/release/cjsh"
```

If you touch parser, interpreter, job control, interactive input, or other memory-sensitive runtime code, also test the debug preset:

```bash
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
./tests/run_shell_tests.sh "build/debug/cjsh"
```

The shell test harness is the broadest local regression check and is the best default verification step before opening a pull request.

## Code Style

- C sources must remain compatible with ISO C11.
- C++ sources must remain compatible with ISO C++17.
- Format touched C and C++ files with `clang-format` using the repository's `.clang-format` file.
- Follow the existing naming, file layout, and style conventions in the area you are modifying.
- Keep changes as small as practical. Small, well-scoped patches are easier to review and safer to merge.

## Tests

Add or update tests when you change behavior.

- Use `tests/shell/` for end-to-end shell behavior and scripting regressions.
- Use the focused C, C++, and Python tests under `tests/` for subsystem-specific coverage.
- If you fix a bug, add a regression test whenever practical.

## Documentation

Update the docs when you change user-visible behavior, builtins, flags, completion behavior, or interactive features.

To preview the docs locally:

```bash
python3 -m pip install -r docs/requirements.txt
mkdocs serve --config-file docs/mkdocs.yml
```

## Pull Requests

When opening a pull request:

- Explain the user-visible change and why it is needed.
- List the commands you ran locally.
- Note any platform-specific testing you performed.
- Include screenshots or terminal recordings for prompt, editing, highlighting, or other UI-facing changes when helpful.

## Continuous Integration

Pull requests and pushes to `master` run the GitHub Actions workflows in `.github/workflows/`. Keep local verification aligned with the parts of CI your change is expected to affect.

## License

By contributing to this repository, you agree that your contributions will be licensed under the project's MIT License.
