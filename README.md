![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7)](https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
![License](https://img.shields.io/badge/License-MIT-green)

## CJ's Shell

Are you scared of the command line? Does working in a terminal bring you dread? Then I have the thing for you!

This is CJ's Shell! This is a custom login shell that I wrote after I got fed up with bash's un-usable customazation, oh-my-zsh's bloat and overall sluggishness, zsh's challening customization making you rely on frameworks to install and build, and fish's small community making issues or bugs hard to diagnose or seek help with and its comes with own propietary scripting language for you to have to learn. And throughout all of this, plugins and themes were always a pain on all 3! CJ's Shell aims to fix all of these for you, and I should know. I use CJ's Shell (cjsh) everyday without the assistance or backup of another shell open. While I do have zsh, bash, fish, etc installed on my machine I do not rely on them to make cjsh function or to help me get any work done. I hope you give cjsh a shot and I hope you enjoy! Thank you!
 
-Caden 'cj' Finley

> ⚠️ **WARNING**: CJ's Shell is not a 100% POSIX compliant shell. I also would not use it as your primary shell script interpreter.

## What is CJ's Shell?

CJ's Shell (cjsh) is a custom login shell with out of the box power. It comes with features like a built-in AI assistant who only offers help when you ask for it, a powerful plugin engine with a versatile language-agnostic plugin API, highly customizable themes, and a vibrant color engine.

### Why Should You Choose CJ's Shell?

- **Built-in AI Assistant**: First-class AI integration with context-aware code assistance, file searching, and multi-model support directly in your terminal—no external plugins required.
- **Dynamic Plugin Engine**: Language-agnostic plugin API with compiled shared libraries, centralized management, and event hooks for deep integration—beyond just "sourced" scripts.
- **Rich Theme System**: JSON-based themes with segmented prompts, dynamic variables (git status, CPU/memory usage), and aligned fields managed through a built-in theme command.
- **Modern Interactive Experience**: Syntax highlighting, programmable completions, and clean configuration files with clear separation between login and interactive settings.

### Why Developers Should Choose cjsh

- **Streamlined Workflow**: AI assistance and rich tooling directly in your terminal means fewer context switches and external tools.
- **Extensibility**: The formal plugin API lets you create powerful integrations in multiple languages, not just shell scripts.
- **Modern UX with Familiar Syntax**: Advanced features without leaving the POSIX syntax you already know.
- **Consistent Cross-Platform Experience**: Works consistently across macOS, Linux, and Windows (via WSL or Cygwin).
- **Customization Without Configuration Pain**: Structured JSON themes and clean plugin management eliminate aannoying shell scripts.

## Installation

### Homebrew Installation (Recomended)

To install cjsh run:
```bash
brew install cadenfinley/tap/cjsh
```

### Manual Build and Install

For manual build and installation, you can build and install from a release on GitHub:

1. Download the source code from the [Releases page](https://github.com/CadenFinley/CJsShell/releases).
2. Extract the archive and navigate to the project directory.
3. Build the project:

   ```bash
   mkdir build && cd build
   cmake ..
   make -j
   ```
4. Move the `cjsh` binary to a directory in your `PATH` (e.g., `/usr/local/bin`):

   ```bash
   sudo mv build/cjsh /usr/local/bin/
   ```

5. Verify that `cjsh` is accessible from anywhere by running:

   ```bash
   cjsh --version
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
- `-c`, `--command` Execute a command and exit
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
- `theme [subcommand]` Manage themes (`list`, `set`, `current`, `preview`, `install`, `uninstall`, etc.)  
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
- `jobs` List all active jobs with their status
- `fg [JOB_ID]` Bring a job to the foreground
- `bg [JOB_ID]` Resume a stopped job in the background

### Configuration Files
- `~/.cjprofile` Login‑mode setup (env vars, PATH, startup args)  
- `~/.cjshrc` Interactive‑mode setup (aliases, functions, themes, plugins)  
- `~/.config/cjsh/plugins/` Directory for installed plugins
- `~/.config/cjsh/themes/` Directory for installed themes
- `~/.config/cjsh/ai/` Directory for AI assistant configurations

### Testing and POSIX Compliance

CJ's Shell includes a comprehensive test suite with over 200 POSIX compliance tests to ensure reliability and standards compliance:

```bash
# Run the complete test suite
./tests/run_shell_tests.sh
```

**POSIX Compliance Status: ~90%**
- ✅ Core shell features (command execution, pipelines, I/O redirection)
- ✅ Environment and variable handling
- ✅ Login shell initialization
- ✅ Variable expansion and quoting
- ✅ Built-in commands and job control
- ✅ Signal handling (basic support)
- ⚠️ Advanced file descriptor operations (noclobber, tab stripping)
- ⚠️ Some interactive-only features require terminal sessions

See `tests/README.md` for detailed test documentation and compliance matrix.

### AI Assistant
The built-in AI assistant provides intelligent help for shell usage and programming tasks:

- `ai` - Enter AI chat mode
- `ai log` - Save chat history to a file
- `ai apikey` - Show API key status
- `ai file` - Manage context files (add, remove, list)
- `ai model [name]` - View or set the AI model
- `ai config` - Manage AI configurations
- `ai voice` - Configure AI voice dictation
- `ai mode [TYPE]` - Set or view the assistant mode
- `ai chat` - Access AI chat commands (history, cache)
- `ai get [KEY]` - Retrieve specific response data
- `ai dump` - Display all response data and last prompt
- `ai directory` - Manage save directory for AI-generated files
- `ai rejectchanges` - Reject AI suggested code changes
- `ai timeoutflag [SECS]` - Set timeout duration for AI requests

Set your OpenAI API key in the OPENAI_API_KEY environment variable to use the AI assistant.

### Plugins
Manage plugins with the `plugin` command:
- `plugin available` - List all available plugins
- `plugin enabled` - List currently enabled plugins
- `plugin enable [NAME]` - Enable a specific plugin
- `plugin disable [NAME]` - Disable a specific plugin
- `plugin info [NAME]` - Show detailed information about a plugin
- `plugin install [PATH]` - Install a new plugin from the given path
- `plugin uninstall [NAME]` - Remove an installed plugin
- `plugin commands [NAME]` - List commands provided by a plugin
- `plugin settings [NAME]` - View or modify plugin settings
- `plugin enableall` - Enable all available plugins
- `plugin disableall` - Disable all enabled plugins

### Themes
Manage themes with the `theme` command:
- `theme` - Show current theme and list available themes
- `theme [NAME]` - Switch to the specified theme
- `theme list` - List all available themes
- `theme preview [NAME]` - Preview a theme without switching to it
- `theme install [NAME]` - Install a theme from available remote themes
- `theme uninstall [NAME]` - Remove an installed theme
- `theme available` - Show remotely available themes that can be installed

### Tutorial for New Users

CJ's Shell includes an interactive tutorial to help new users get started. This tutorial introduces basic shell commands and features of CJ's Shell in an easy-to-follow manner. To start the tutorial, simply type `tutorial` in the shell. You can skip the tutorial at any time by typing `tutorial skip`.

## Third‑Party Components

- **isocline**  
  https://github.com/daanx/isocline (MIT License)
 
- **nlohmann/json**  
  https://github.com/nlohmann/json (MIT License)
 
- **curl**  
  https://github.com/curl/curl (MIT License)

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
cadenfinley.com
caden@cadenfinley.com
