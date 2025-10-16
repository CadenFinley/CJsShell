# CJ's Shell <a href="https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master"><img src="https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true" alt="Build status"></a> <a href="https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7" alt="Codacy Badge"></a> <img src="https://img.shields.io/badge/License-MIT-green" alt="License">

CJ's Shell (cjsh) is a shell with out of the box power and speed without any of the modern shell bloat. Baked in are strong first party theme scripting with a custom theme DSL language, a full custom shell script interpreter with minor bash support, custom keybindings for text editing, custom syntax highlighting, fuzzy text auto completions, smart directory navigation, advanced history seaching, multiline editing, typeahead, and rich prompts. All with no external shell support and only 1 dependency which is already baked in. cjsh aims to be fast and responsive at all times. It is fully usable on all *nix like systems and Windows with WSL. cjsh aims to be an almost 1 to 1 switch over from other POSIX like shells.

📖 **[Full Documentation](https://cadenfinley.github.io/CJsShell/)**

## Quick Install (Recommended)

The easiest way to get started with CJ's Shell is with brew
- **[Getting Started](docs/getting-started/quick-start.md)**

```bash
brew tap CadenFinley/tap
brew install cjsh
```
   
# Testing and POSIX Compliance

CJ's Shell includes a comprehensive test suite with over 1000 POSIX compliance tests to ensure reliability and standards compliance.
This test can be run from the root of the repository, and is recommended to run before setting cjsh as your default login shell.

```bash
./tests/run_shell_tests.sh
```

# Third‑Party Components

- **isocline**
  https://github.com/daanx/isocline (MIT License)

> **ALERT** This library has been HEAVILY modified and is far different from its original state.

# License

This project is licensed under the MIT License.

# Author

**Caden Finley** @ Abilene Christian University
© 2025

- Website: [cadenfinley.com](https://cadenfinley.com)
- Email: [caden@cadenfinley.com](mailto:caden@cadenfinley.com)