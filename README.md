# CJ's Shell <a href="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml"><img src="https://github.com/CadenFinley/CJsShell/actions/workflows/ci.yml/badge.svg" alt="Build"></a> <a href="https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7" alt="Codacy Badge"></a> <a href="https://cadenfinley.github.io/CJsShell/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-blue" alt="Documentation"></a> <img src="https://img.shields.io/badge/License-MIT-green" alt="License">

CJ's Shell (cjsh) is a POSIX-based interactive shell that pairs familiar script compatibility with integrated modern features. Built in are first-party theme scripting with a custom DSL, a POSIX shell interpreter with bash extensions, customizable keybindings, syntax highlighting, fuzzy completions, smart directory navigation, advanced history search, multiline editing, typeahead, and rich prompts. Everything ships in one binary with a single vendored dependency, so cjsh works out of the box on all *nix-like systems and Windows via WSL. cjsh delivers a POSIX+ experience, standard scripting semantics with an enhanced interactive layer you can dial up or down as needed.

The scripting core targets roughly 95% POSIX coverage so existing shell scripts work as expected, while POSIX+ enhancements amplify the interactive experience without requiring external plugins.

> **WARNING** cjsh is still in active development. for the most stable versions, stick to tagged releases or releases through brew. there are known scripting edge cases and bugs.

## Quick Install (Recommended)

The easiest way to get started with CJ's Shell is with brew. For detailed installation instructions and getting started guides, visit the **[Getting Started](https://cadenfinley.github.io/CJsShell/getting-started/quick-start/)** page.

```bash
brew tap CadenFinley/tap
brew install cjsh
```

## Building from source

Any non tagged releases or commits do not have the promise of not containing non breaking changes or working builds. For maximum build safety and usability, stick to tagged releases or through package manager installs.

To build from source:
```bash
git clone https://github.com/CadenFinley/CJsShell && cd CJsShell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
   
# Scripting Compatibility & Testing

CJ's Shell backs its POSIX-based scripting engine with over 1500 targeted tests that validate standards-aligned behavior while covering common bash extensions.
Run the suite from the repository root before switching cjsh to your default login shell.

```bash
./tests/run_shell_tests.sh
```

# Third‑Party Components

- **isocline**
  https://github.com/daanx/isocline (MIT License)

> **ALERT** This library has been HEAVILY modified and is far different from its original state. To see the actual isocline implementation that cjsh uses see: https://github.com/CadenFinley/isocline

# License

This project is licensed under the MIT License.

# Author

**Caden Finley** @ Abilene Christian University
© 2025

- Website: [cadenfinley.com](https://cadenfinley.com)
- Email: [caden@cadenfinley.com](mailto:caden@cadenfinley.com)