# CJ's Shell (cjsh)

[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
![Lines of Code](https://img.shields.io/badge/lines%20of%20code-21098-green)

> ⚠️ **WARNING**: This project is still in active development. There are known bugs and issues that are being addressed. Use at your own risk.

## Installation

### Quick Installation

```bash
curl -fsSL https://raw.githubusercontent.com/CadenFinley/CJsShell/master/tool-scripts/cjsh_install.sh | bash
```

To install and set as your default shell:

```bash
curl -fsSL https://raw.githubusercontent.com/CadenFinley/CJsShell/master/tool-scripts/cjsh_install.sh | bash -s -- --set-as-shell
```

### Manual Installation

You can also build and install from source:

```bash
# Clone the repository
git clone https://github.com/CadenFinley/CJsShell.git
cd CJsShell

# Build the project
mkdir build && cd build
cmake ..
make
```

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
