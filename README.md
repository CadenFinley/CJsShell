![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7)](https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
![License](https://img.shields.io/badge/License-MIT-green)

# CJ's Shell

Are you scared of the command line? Does working in a terminal bring you dread? Then I have the thing for you!

This is CJ's Shell! This is a fully custom login shell that I wrote after I got fed up with bash's un-usable customazation, oh-my-zsh's bloat and overall sluggishness, zsh's challening customization making you rely on frameworks to install and build, and fish's small community making issues or bugs hard to diagnose or seek help with and its comes with own propietary scripting language for you to have to learn. And throughout all of this, plugins and themes were always a pain on all 3! CJ's Shell aims to fix all of these for you, and I should know. I use CJ's Shell (cjsh) everyday without the assistance or backup of another shell open. While I do have zsh, bash, fish, etc installed on my machine I do not rely on them to make cjsh function or to help me get any work done. I hope you give cjsh a shot and I hope you enjoy! Thank you!
 
-Caden 'cj' Finley

> **WARNING**: CJ's Shell is not a 100% POSIX compliant shell. I also would not use it as your primary shell script interpreter. It is also super cool and you will never want to use another shell again.

## Quick Install (Recommended)

The easiest way to install CJ's Shell is with brew

```bash
    brew tap CadenFinley/tap
    brew install cjsh
```

## Manual Installation

To manually build cjsh:

1. Clone the repository:
```bash
    git clone https://github.com/CadenFinley/CJsShell.git
```
2. Change directory to the project:
```bash
    cd CJsShell
```
3. Build the project:
```bash
   ./toolchain/build.sh
```
   
# Testing and POSIX Compliance

CJ's Shell includes a comprehensive test suite with over 900 POSIX compliance tests to ensure reliability and standards compliance.
This test can be ran from the root of the repository, and is recommended to run before setting cjsh as you default login shell.

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
