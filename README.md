<!--
  README.md

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

# CJ's Shell (cjsh) <a href="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml"><img src="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml/badge.svg" alt="CI"></a> <a href="https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7" alt="Codacy Badge"></a> <a href="https://cadenfinley.github.io/CJsShell/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Documentation"></a> <img src="https://img.shields.io/github/repo-size/CadenFinley/CJsShell" alt="Repo Size">

<p align="center"><strong>POSIX-first scripting with a modern interactive shell experience</strong></p>
<p align="center"><img src="docs/cjsh_recording.svg" alt="Terminal recording showing CJ's Shell features"></p>

`cjsh` is a POSIX-first shell with an enhanced interactive layer. It combines standards-oriented scripting behavior with modern shell features such as rich completions, customizable keybindings, syntax highlighting, multiline editing, prompt styling, spell correction, and history search.

The project builds into a single `cjsh` binary and vendors its line-editing dependency, so the shell works out of the box without an external plugin stack or framework.

> `cjsh` is under active development. For the most stable experience, prefer tagged releases or package-manager builds.

## Install

Detailed install and onboarding guides are available in the [documentation site](https://cadenfinley.github.io/CJsShell/getting-started/quick-start/).

### Homebrew (macOS/Linux)
```bash
brew tap CadenFinley/tap
brew install cjsh
```

### Arch Linux (AUR)
```bash
# Using yay
yay -S cjsh

# Using paru
paru -S cjsh
```

## Build From Source

### Requirements

- C compiler
- C++ compiler
- CMake 3.25 or newer
- Ninja
- Python 3 for parts of the test suite

### Build

```bash
git clone https://github.com/CadenFinley/CJsShell && cd CJsShell
cmake --preset release
cmake --build --preset release --parallel
```

Run the shell with:

```bash
./build/release/cjsh
```

Install it under a prefix with:

```bash
cmake --install build/release --prefix ~/.local
```

### Available Presets

- `release`: optimized default build
- `debug`: debug build with AddressSanitizer enabled
- `minimal`: size-focused release profile
- `relwithdebinfo`: optimized build with debug symbols
- `minsizerel`: CMake `MinSizeRel` profile

List all configured presets with `cmake --list-presets`.

## Testing

The repository includes both focused CTest coverage and a larger shell integration harness.

Run the registered CTest suites:

```bash
ctest --preset release
```

Run the full shell and integration harness from the repository root:

```bash
./tests/run_shell_tests.sh "build/release/cjsh"
```

If you are working on parser, runtime, job-control, or memory-sensitive changes, also build and test the `debug` preset.

## Documentation

Project documentation is published at [cadenfinley.github.io/CJsShell](https://cadenfinley.github.io/CJsShell/).

To preview the docs locally:

```bash
python3 -m pip install -r docs/requirements.txt
mkdocs serve --config-file docs/mkdocs.yml
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup, formatting, testing, and pull request expectations.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
