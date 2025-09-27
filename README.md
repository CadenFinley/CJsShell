![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Tests](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true&passingText=tests%20passing&failingText=tests%20failing&pendingText=tests%20pending)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7)](https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
![License](https://img.shields.io/badge/License-MIT-green)

# CJ's Shell

Are you scared of the command line? Does working in a terminal bring you dread? Then I have the thing for you!

This is CJ's Shell! This is a fully custom login shell that I wrote after I got fed up with bash's un-usable customazation, oh-my-zsh's bloat and overall sluggishness, zsh's challening customization making you rely on frameworks to install and build, and fish's small community making issues or bugs hard to diagnose or seek help with and its comes with own propietary scripting language for you to have to learn. And throughout all of this, plugins and themes were always a pain on all 3! CJ's Shell aims to fix all of these for you, and I should know. I use CJ's Shell (cjsh) everyday without the assistance or backup of another shell open. While I do have zsh, bash, fish, etc installed on my machine I do not rely on them to make cjsh function or to help me get any work done. I hope you give cjsh a shot and I hope you enjoy! Thank you!
 
-Caden 'cj' Finley

> **WARNING**: CJ's Shell is not a 100% POSIX compliant shell. I also would not use it as your primary shell script interpreter.

## What is CJ's Shell?

CJ's Shell (cjsh) is a custom login shell with out of the box power. It comes with features like a built-in AI assistant who only offers help when you ask for it, a powerful plugin engine with a versatile language-agnostic plugin API, highly customizable themes, and a vibrant color engine.

## Why Should You Choose CJ's Shell?

- **Built-in AI Assistant**: First-class AI integration with context-aware code assistance, file searching, and multi-model support directly in your terminal; no external plugins required.
- **Dynamic Plugin Engine**: Language-agnostic plugin API with compiled shared libraries, centralized management, and event hooks for deep integration—beyond just "sourced" scripts.
- **Rich Theme System**: JSON-based themes with segmented prompts, dynamic variables (git status, CPU/memory usage), and aligned fields managed through a built-in theme command.
- **Modern Interactive Experience**: Syntax highlighting, rich, comprehensive, and dynamic completions, and clean configuration files with clear separation between login and interactive settings.

## Why Developers Should Choose cjsh

- **Streamlined Workflow**: AI assistance and rich tooling directly in your terminal means fewer context switches and external tools.
- **Extensibility**: The formal plugin API lets you create powerful integrations in multiple languages, not just shell scripts.
- **Modern UX with Familiar Syntax**: Advanced features without leaving the POSIX syntax you already know.
- **Consistent Cross-Platform Experience**: Works consistently across macOS, Linux, and Windows (via WSL or Cygwin).
- **Customization Without Configuration Pain**: Structured JSON themes and clean plugin management eliminate aannoying shell scripts.

# Installation

## Quick Install (Recommended)

The easiest way to install CJ's Shell is using the automatic installer:

```bash
curl -fsSL https://raw.githubusercontent.com/CadenFinley/CJsShell/master/tool_scripts/install.sh | bash
```

## Manual Installation

For latest stable patch, you can build and install manually from a release on GitHub:

1. Download the source code from the [Releases page](https://github.com/CadenFinley/CJsShell/releases).
2. Extract the archive and navigate to the project directory.
3. Build the project:

   ```bash
   ./tool_scripts/build.sh
   ```

## Uninstalling cjsh
It is as simple as running a command within the shell in interactive mode:

```bash
cjsh_uninstall
```
   
# Testing and POSIX Compliance

CJ's Shell includes a comprehensive test suite with over 900 POSIX compliance tests to ensure reliability and standards compliance.
This test can be ran from the root of the repository, and is recommended to run before setting cjsh as you default login shell.

```bash
# Run the complete test suite
./tests/run_shell_tests.sh
```

# Third‑Party Components

- **isocline**  
  https://github.com/daanx/isocline (MIT License)
 
- **nlohmann/json**  
  https://github.com/nlohmann/json (MIT License)

- **utf8proc**  
  https://github.com/JuliaStrings/utf8proc (MIT License)

# License

This project is licensed under the MIT License.

# Author

**Caden Finley** @ Abilene Christian University  
© 2025

- Website: [cadenfinley.com](https://cadenfinley.com)
- Email: [caden@cadenfinley.com](mailto:caden@cadenfinley.com)
