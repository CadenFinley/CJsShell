```plaintext
   ______    _______ __  __
  / ____/   / / ___// / / /
 / /   __  / /\__ \/ /_/ / 
/ /___/ /_/ /___/ / __  /  
\____/\____//____/_/ /_/   
```

![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7)](https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
![License](https://img.shields.io/badge/License-MIT-green)

## CJ's Shell

CJ's Shell (cjsh) is a custom login shell with out of the box power. It comes with features like a built-in AI assistant who only offers help when you ask for it, a powerful plugin engine with a versatile language-agnostic plugin API, highly customizable themes, and a vibrant color engine.

> ⚠️ **WARNING**: This project is still in active development. There may be occasional bugs. Please report any issues you encounter by opening a pull request or issue.

## Why Choose CJ's Shell?

### Comparing cjsh with bash, zsh, and fish

CJ's Shell combines the familiarity of traditional POSIX shells with modern developer-focused features:

#### Key Differences

- **Built-in AI Assistant**: First-class AI integration with context-aware code assistance, file searching, and multi-model support directly in your terminal—no external plugins required.
- **Dynamic Plugin Engine**: Language-agnostic plugin API with compiled shared libraries, centralized management, and event hooks for deep integration—beyond just "sourced" scripts.
- **Rich Theme System**: JSON-based themes with segmented prompts, dynamic variables (git status, CPU/memory usage), and right-aligned fields managed through a built-in theme command.
- **Batteries Included**: Built-in commands for enhanced `ls`, version checking, updates, and plugin/theme management without external dependencies.
- **Modern Interactive Experience**: Syntax highlighting, programmable completions, and clean configuration files with clear separation between login and interactive settings.
- **Self-Updating**: Built-in `--update` and `--check-update` commands to stay current without relying on package managers.

#### Core Similarities

CJ's Shell maintains compatibility with familiar shell concepts:
- Standard tokenization, quoting, and escaping
- Environment variable expansion and tilde expansion
- Pipelines, I/O redirection, and background jobs
- Logical operators and command separators
- Basic shell functions and aliases
- Core built-ins like `cd`, `exit`, `export`, and `history`

### Why Developers Choose cjsh

- **Streamlined Workflow**: AI assistance and rich tooling directly in your terminal means fewer context switches and external tools.
- **Extensibility**: The formal plugin API lets you create powerful integrations in multiple languages, not just shell scripts.
- **Modern UX with Familiar Syntax**: Enjoy advanced features without abandoning the POSIX syntax you already know.
- **Consistent Cross-Platform Experience**: Works consistently across macOS, Linux, and Windows (via WSL).
- **Customization Without Configuration Hell**: Structured JSON themes and clean plugin management eliminate fragile shell script hacks.

## Installation

### MACOS Install

This installs cjsh from a custom brew tap hosted at: https://github.com/CadenFinley/homebrew-tap

```bash
brew install cadenfinley/tap/cjsh
```

### LINUX Install

To install cjsh on your Linux distribution:

1. First follow the manual installation steps below to build the project and then navigate to the tool-scripts directory in the root of the repo
2. Make the installation script executable:
   ```bash
   chmod +x ./tool-scripts/linux_install_from_local.sh
   ```
3. After the build is complete, run the installation script with sudo privileges:
   ```bash
   sudo ./tool-scripts/linux_install_from_local.sh
   ```

This script will:
- Install the cjsh binary to `/usr/local/bin/`
- Install the man page if available
- Create the necessary `.config/cjsh` directory structure in your home folder
- Show instructions for setting cjsh as your default shell

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

## Usage

### Launching cjsh
Start an interactive session:
```bash
cjsh
```

Start a login session:
```bash
cjsh --login
```
Or run a one‑off command:
```bash
cjsh -c "ls -la"
```
Available startup flags:
- `-l`, `--login` Start in login mode  
- `-i`, `--interactive` Force interactive mode
- `--set-as-shell` Show instructions to set cjsh as your login shell  
- `--no-update` Disable automatic update checks
- `--check-update` Enable update checks
- `--update` Check for updates and exit
- `-d`, `--debug` Enable debug logging  
- `--no-plugins` Disable the plugin system
- `--no-themes` Disable theme support
- `--no-ai` Disable AI assistant features
- `--no-colors` Disable color output
- `--no-titleline` Disable title line display
- `--no-source` Don't source the ~/.cjshrc file
- `--silent-updates` Perform update checks silently
- `-v`, `--version` Show version information and exit
- `-h`, `--help` Show help information and exit

### Common Built‑In Commands
- `help` Display help and usage information  
- `cd [dir]` Change directory  
- `export VAR=val` Set environment variables  
- `plugin [subcommand]` Manage plugins (`available`, `enable`, `disable`, `install`, `uninstall`, etc.)  
- `theme [subcommand]` Manage themes (`list`, `set`, `current`, etc.)  
- `aihelp` Invoke the AI assistant  
- `ai [command]` Use the AI assistant with specific commands
- `version` Show the current cjsh version  
- `ls [options] [path]` List directory contents with enhanced formatting
- `alias [name[=value]]` Create command aliases
- `history` View and manage command history
- `eval [expression]` Evaluate a shell expression
- `restart` Restart the shell
- `uninstall` Remove cjsh from your system
- `approot` Display the application installation directory
- `user` Manage user configuration

### Configuration Files
- `~/.cjprofile` Login‑mode setup (env vars, PATH, startup args)  
- `~/.cjshrc` Interactive‑mode setup (aliases, functions, themes, plugins)  
- `~/.config/cjsh/plugins/` Directory for installed plugins
- `~/.config/cjsh/themes/` Directory for installed themes
- `~/.config/cjsh/ai/` Directory for AI assistant configurations

### AI Assistant
The built-in AI assistant provides intelligent help for shell usage and programming tasks:

- `ai` - Enter AI chat mode
- `ai log` - Save chat history to a file
- `ai apikey` - Show API key status
- `ai file` - Manage context files (add, remove, list)
- `ai model [name]` - View or set the AI model
- `ai config` - Manage AI configurations
- `ai voice` - Configure AI voice dictation

Set your OpenAI API key in the OPENAI_API_KEY environment variable to use the AI assistant.

### Plugins
Manage plugins with the `plugin` command:
- `plugin available` - List all available plugins
- `plugin enabled` - List currently enabled plugins
- `plugin enable [NAME]` - Enable a specific plugin
- `plugin disable [NAME]` - Disable a specific plugin
- `plugin info [NAME]` - Show detailed information about a plugin
- `plugin install [PATH]` - Install a new plugin from the given path

### Themes
Manage themes with the `theme` command:
- `theme` - Show current theme and list available themes
- `theme [NAME]` - Switch to the specified theme
- `theme list` - List all available themes

## Third‑Party Components

- **isocline**  
  https://github.com/daanx/isocline (MIT License)
 
- **nlohmann/json**  
  https://github.com/nlohmann/json (MIT License)
 
- **libcurl**  
  https://github.com/curl/curl (MIT License)
 
- **OpenSSL** (optional, for SSL support)  
  https://www.openssl.org (Apache License 2.0)
  
- **Vosk** (optional, for voice transcription)  
  https://github.com/alphacep/vosk-api (Apache License 2.0)

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
