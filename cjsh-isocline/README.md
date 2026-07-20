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

# cjsh-isocline <a href="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml"><img src="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml/badge.svg" alt="CI"></a> <a href="https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7" alt="Codacy Badge"></a> <a href="https://cadenfinley.github.io/CJsShell/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Documentation"></a> <img src="https://img.shields.io/github/repo-size/CadenFinley/CJsShell" alt="Repo Size">

<p align="center"><strong>Vendored and extended isocline engine for CJ's Shell</strong></p>

This directory contains the `cjsh_isocline` static library used by `cjsh` for interactive terminal input. It is based on isocline and has been significantly adapted for CJ's Shell features such as advanced keybinding hooks, status messaging, prompt-line behaviors, and shell-specific integration paths.

The target is built as ISO C11, compiles with `IC_SEPARATE_OBJS=1`, and exports headers (including a generated compatibility include path) for the shell runtime.

> **WARNING** this fork intentionally diverges from upstream isocline to support cjsh behavior. prefer tagged CJ's Shell releases for stability.

## Build this module through the root project

This module is expected to be built from the repository root so shared build flags, metadata, and threading configuration are applied.

```bash
git clone https://github.com/CadenFinley/CJsShell && cd CJsShell
cmake --preset release
cmake --build --preset release --parallel
```

## Module layout

- `src/isocline.h` - public API surface consumed by `cjsh`
- `src/core/` - aggregation unit, runtime environment lifecycle, and shared core helpers
- `src/edit/` - readline entry points, interactive editing flow, typeahead, history, and undo
- `src/completion/` - completion and highlighting engine internals
- `src/keybinding/` - keycode tables and keybinding registration/runtime behavior
- `src/terminal/` - terminal rendering, bbcode/attributes, TTY I/O, and Unicode helpers
- `src/utils/` - reusable utilities shared across components

## Upstream and license

This module is distributed under the MIT License as part of CJ's Shell.
It includes work from the original isocline project by Daan Leijen, with extensive cjsh-focused modifications.

**Caden Finley** @ Abilene Christian University
© 2026
