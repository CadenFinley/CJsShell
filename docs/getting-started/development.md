<!--
  development.md

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

# Development

Want to help with `cjsh`? This page is the quickest path from clone to a tested local build. For the full contributor workflow, see the repository's [CONTRIBUTING.md](https://github.com/CadenFinley/CJsShell/blob/master/CONTRIBUTING.md).

## Requirements

- C compiler
- C++ compiler
- CMake 3.25 or newer
- Ninja
- Python 3 for parts of the test suite

## Build

From the repository root:

```bash
git clone https://github.com/CadenFinley/CJsShell && cd CJsShell
cmake --preset release
cmake --build --preset release --parallel
```

Run the shell with:

```bash
./build/release/cjsh
```

Useful presets:

- `release`: optimized default build
- `debug`: debug build with AddressSanitizer enabled
- `minimal`: size-focused release profile

## Test

Run the focused CTest suites:

```bash
ctest --preset release
```

Run the full shell and integration harness:

```bash
./tests/run_shell_tests.sh "build/release/cjsh"
```

If you are changing parser behavior, runtime execution, job control, or interactive input handling, also build and test the `debug` preset.

## Work On Docs

Documentation contributions are always useful, especially when they clarify behavior, explain configuration, or document new builtins and options.

To preview the docs locally:

```bash
python3 -m pip install -r docs/requirements.txt
mkdocs serve --config-file docs/mkdocs.yml
```

## Themes And Prompt Styling

Prompt, theme, and styling contributions are welcome. If you have a `PS1`, `RPS1`, or `cjshopt style_def` setup worth sharing, open a pull request that adds it to the docs or examples. The [Prompt Markup and Styling](../themes/thedetails.md) guide covers the available knobs.
