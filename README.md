# CJ's Shell (cjsh)

[![Build status](https://ci.appveyor.com/api/projects/status/dqk13klgh9d22bu5?svg=true)](https://ci.appveyor.com/project/CadenFinley/CJsShell)
![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
![Lines of Code](https://img.shields.io/badge/lines%20of%20code-10260-green)


CJ's Shell (cjsh) is a powerful shell environment for developers that combines traditional shell functionality with AI assistance. It can function as a complete login shell replacement or as a regular application. Beyond standard terminal capabilities, it provides enhanced productivity features including custom shortcuts, AI-powered assistance, plugin support, and theme customization.

## Key Features

- **Versatile Shell Environment**: Works as both a standalone application and a complete login shell replacement
- **OpenAI Integration**: Direct access to GPT models for code explanations, problem-solving, and documentation
- **Workflow Automation**: Create multi-command shortcuts and custom aliases to streamline repetitive tasks
- **Plugin System**: Extend functionality with plugins for specialized development tasks
- **Theme Customization**: Personalize your terminal appearance with custom themes
- **Smart Tab Completion**: Context-aware command and file completion
- **Automatic Updates**: Stay current with automatic update checks and easy installation
- **Job Control**: Full support for background processes and job management

## Using as a Login Shell

CJ's Shell can replace your system's default shell (bash, zsh, etc.) for a seamless experience. When used as a login shell, it:

- Processes standard profile files (/etc/profile, ~/.profile, ~/.bashrc, etc.)
- Configures PATH variables with automatic Homebrew path detection
- Manages proper job control and signal handling
- Handles interactive commands requiring password input (sudo, ssh, etc.)
- Preserves all standard shell functionality while adding AI capabilities

## Installation

### Quick Install (macOS/Linux)

```sh
# Default installation
curl -sL https://raw.githubusercontent.com/cadenfinley/CJsShell/main/tool-scripts/cjsh-install.sh | bash
```

```sh
# Install AND set as default shell
curl -sL https://raw.githubusercontent.com/cadenfinley/CJsShell/main/tool-scripts/cjsh-install.sh | bash -s -- --set-as-shell
```

### Manual Installation

1. Download the latest release from the [GitHub Releases page](https://github.com/cadenfinley/CJsShell/releases)
2. Extract the package and run the installer:
   ```sh
   chmod +x cjsh-install.sh
   ./cjsh-install.sh [--set-as-shell]
   ```

3. The installer will:
   - Create the necessary directory structure in `~/.cjsh_data`
   - Install the executable and create a system-wide link
   - Register CJ's Shell as a valid login shell
   - Optionally set it as your default shell
   - Configure your environment for optimal use

### Switching Between Shells

To set CJ's Shell as your default shell:
```sh
chsh -s /usr/local/bin/cjsh
```

To revert to your previous shell:
```sh
# Use your backed-up shell preference
chsh -s $(cat ~/.cjsh_data/original_shell.txt)
```

## Usage

### Basic Commands

- `!help` - Display help information
- `!terminal [command]` - Execute terminal commands
- `!ai [message]` - Interact with the AI assistant
- `!user` - Access user settings
- `!theme` - Manage themes
- `!plugin` - Manage plugins

### AI Integration

Set up OpenAI integration:
```sh
!ai apikey set YOUR_API_KEY
```

Ask for AI assistance:
```sh
!ai How do I implement a binary search in C++?
```

Get AI troubleshooting help:
```sh
!aihelp
```

### Workflow Automation

Create shortcuts for multiple commands:
```sh
!user shortcut add gs git status; git diff --stat
```

Use the shortcut:
```
-gs
```

Create aliases:
```sh
!user alias add ll ls -la
```

## Configuration

CJ's Shell stores configuration in `~/.cjsh_data/USER_DATA.json`. You can customize:

- Startup commands
- Default theme
- AI settings
- Update preferences
- Input/output behavior

## Plugin Development

CJ's Shell supports a plugin system for extending functionality. To develop plugins:

1. Use the C++ plugin API in the `src/include/plugininterface.h` file
2. Compile your plugin as a shared library (.so/.dylib)
3. Place in the `~/.cjsh_data/plugins` directory
4. Enable with `!plugin enable [plugin-name]`

## Building from Source

Requirements:
- CMake 3.10+
- C++17 compatible compiler
- nlohmann/json library

Build steps:
```sh
git clone https://github.com/cadenfinley/CJsShell.git
cd CJsShell
mkdir build && cd build
cmake ..
make
```

## Uninstallation

```sh
# From within CJ's Shell
!uninstall

# Or run the uninstaller directly
~/.cjsh_data/cjsh_uninstall.sh [--all]
```

The `--all` flag removes all user data including settings, history, and plugins.

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
