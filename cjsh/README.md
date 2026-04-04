# cjsh Core <a href="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml"><img src="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml/badge.svg" alt="CI"></a> <a href="https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7" alt="Codacy Badge"></a> <a href="https://cadenfinley.github.io/CJsShell/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Documentation"></a> <img src="https://img.shields.io/github/repo-size/CadenFinley/CJsShell" alt="Repo Size">

<p align="center"><strong>The POSIX+ shell runtime that powers CJ's Shell</strong></p>
<img align="center" src="../docs/cjsh_recording.svg"/>

This directory contains the core `cjsh` executable and `cjsh_core` static library. It includes the parser, interpreter, builtins, completion system, syntax highlighting, validation pipeline, prompt layer, and startup/runtime filesystem integration used by CJ's Shell.

`cjsh` is built as a C++17 target and links against `cjsh_isocline` for interactive line editing while preserving POSIX-first scripting behavior with bash-style extensions.

> **WARNING** this module is under active development and is expected to move quickly. for the most stable experience, use tagged releases from the root project.

## Build this module through the root project

This subdirectory is designed to be built from the repository root so shared build metadata and settings are applied consistently.

```bash
git clone https://github.com/CadenFinley/CJsShell && cd CJsShell
cmake --preset release
cmake --build --preset release --parallel
```

## Core layout

- `src/cjsh.cpp` - entrypoint and startup/shutdown flow
- `src/parser` - command language parsing and AST building
- `src/interpreter` - execution engine and runtime evaluation
- `src/builtin` - shell builtins and builtin option handling
- `src/completions` / `src/highlighter` - interactive UX helpers
- `src/validation` - command and syntax validation passes

## Testing

Run shell tests from the repository root:

```bash
./tests/run_shell_tests.sh
```

## License

This module is part of CJ's Shell and is licensed under the MIT License.

**Caden Finley** @ Abilene Christian University
© 2026
