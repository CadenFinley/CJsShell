![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7)](https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
![License](https://img.shields.io/badge/License-MIT-green)

# CJ's Shell

> **WARNING**: CJ's Shell is not a 100% POSIX compliant shell yet. (like 95%) I also would not use it as your primary shell script interpreter for super complex tasks. It is also super cool and you will never want to use another shell again.

## Quick Install (Recommended)

The easiest way to install CJ's Shell is with brew

```bash
    brew tap CadenFinley/tap
    brew install cjsh
```

## Manual Installation

To manually build cjsh:

```bash
    # Clone the repo
    git clone https://github.com/CadenFinley/CJsShell.git

    # Go to the cjsh directory
    cd CJsShell 

    # Build the project:
   ./toolchain/build.sh
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

# License

This project is licensed under the MIT License.

# Author

**Caden Finley** @ Abilene Christian University
© 2025

- Website: [cadenfinley.com](https://cadenfinley.com)
- Email: [caden@cadenfinley.com](mailto:caden@cadenfinley.com)
