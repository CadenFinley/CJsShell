# Features Overview

CJ's Shell is a POSIX-based interactive shell designed to provide a powerful, fast, and customizable experience with minimal configuration required.

## Core Features

### POSIX-Based Scripting
cjsh targets approximately 95% POSIX coverage for scripting so existing shell scripts run as expected while modern conveniences remain available.

- Standards-aligned POSIX shell syntax with common bash extensions
- Compatible with most `sh`/`bash` scripts and tooling expectations
- Backed by 1500+ automated tests that focus on scripting semantics

**See Also**: [POSIX+ Interactive Features](non-posix-features.md) for details on the enhancements layered above the core scripting engine.

### Shell Scripting

#### Conditionals
- `if/elif/else/fi` constructs
- `test`, `[`, and `[[` expressions
- Supports both POSIX and extended test expressions

#### Loops
- `for` loops
- `while` loops
- `until` loops
- `break` and `continue` statements

#### Functions
- POSIX-style function definitions
- Local variables with `local` command
- Return codes and `return` statement

#### Advanced Features
- Command substitution with `$(...)`
- Pipelines and command chaining
- Redirection (stdin, stdout, stderr)
- Here-strings and here-documents
- Background jobs and job control

### Interactive Features

CJ's Shell provides a rich interactive experience powered by the [isocline](https://github.com/cadenfinley/isocline) line editor library.

**See [Interactive Editing Guide](editing.md) for complete documentation of all editing features.**

#### Line Editing
Advanced text editing capabilities including:

- **Multiline input** with intelligent continuation detection
- **Line numbering** (absolute and relative modes)
- **Syntax highlighting** in real-time as you type
- **Context-aware tab completion** with fuzzy matching
- **Incremental history search** (Ctrl+R)
- **Inline hints** and completion preview
- **Visible whitespace markers** for spotting stray spaces
- **Spell correction** for commands and completions
- **Brace matching** and auto-insertion
- **Fish-style abbreviations** with `abbr`/`unabbr` management commands
- **Customizable key bindings** (emacs and vi modes)
- **Interactive help** (F1 for full key binding cheat sheet)

**Configuration:**
All editing features can be configured via the `cjshopt` command:
```bash
cjshopt multiline on|off|status                 # Toggle multiline input
cjshopt multiline-start-lines <count|status>    # Prefill multiline prompts with extra lines
cjshopt line-numbers <relative|absolute|off|status>
cjshopt completion-preview on|off|status        # Show completion preview popups
cjshopt completion-spell on|off|status          # Completion spell correction
cjshopt completion-case on|off|status           # Case-sensitive matching (default off)
cjshopt visible-whitespace on|off|status        # Render whitespace markers
cjshopt hint on|off|status                      # Inline hints
cjshopt hint-delay <milliseconds|status>        # Delay before showing hints
cjshopt auto-tab on|off|status                  # Auto-expand unique completions (default off)
cjshopt keybind profile set emacs               # Select a key binding profile (also supports 'vi')
```

See the [Editing Guide](editing.md) for all available options and detailed usage.

#### Smart Directory Navigation
Enhanced `cd` command with intelligent features:

- Fuzzy directory matching
- Directory bookmarks
- Adjustable bookmark limit with `cjshopt set-max-bookmarks`
- Blacklist unwanted locations via `cjshopt bookmark-blacklist`
- Previous directory switching with `cd -`
- Can be disabled with `--no-smart-cd` flag

#### Auto-completion
Built-in completion system with advanced features:

- **Command completion** from PATH, builtins, aliases, and functions
- **File and directory completion** with intelligent quoting
- **Variable completion** for shell variables
- **User and hostname completion** for ssh, scp, etc.
- **Fuzzy matching** for typo tolerance
- **Frequency-based ranking** (commonly used items first)
- **Source attribution** (shows where completions come from)
- **Spell correction** when no exact match is found
- **Configurable case sensitivity**

See [Editing Guide](editing.md#completion-system) for detailed completion documentation.

#### Syntax Highlighting
Real-time syntax highlighting as you type with full customization:

- **Command recognition** (valid commands, builtins, errors)
- **String highlighting** (single and double quoted)
- **Operator highlighting** (pipes, redirections, logical operators)
- **Keyword highlighting** (if, then, else, while, for, etc.)
- **Variable highlighting** (parameter expansions)
- **Comment highlighting**
- **Brace matching** (matching pairs highlighted)
- **Customizable color schemes** via `cjshopt style_def`

**Example style customization:**
```bash
cjshopt style_def ic-keyword "bold blue"
cjshopt style_def ic-command "green"
cjshopt style_def ic-error "bold red"
cjshopt style_def ic-string "#ffaa00"
```

See [Editing Guide](editing.md#syntax-highlighting) for all available styles and customization options.

### Theming System

#### Custom Theme DSL
Proprietary theme scripting language inspired by JSON and Ruby:

- Data-oriented and hierarchical
- Strongly typed
- Fast evaluation (2-4x faster than alternatives)
- Theme caching for performance

#### Prompt Types
- **PS1**: Default prompt
- **GIT**: Git repository prompt with status information
- **inline_right**: Right-aligned prompt elements
- **newline**: Multi-line prompt support

#### Prompt Information Variables
Extensive set of variables for displaying system information:

- User and system info (username, hostname, path)
- Time and date formatting
- Git repository status and metrics
- Language detection (Python, Node.js, Rust, Go, Java, etc.)
- Container detection (Docker, Podman)
- System resources (CPU, memory, disk usage)
- Network information
- Custom command execution with caching

See the themes documentation for complete variable list.

### Configuration

#### Startup Files
- `~/.cjprofile` - Login configuration and startup flags
- `~/.cjshrc` - Interactive session configuration
- `~/.cjsh_logout` - Optional logout script

**Note**: To load custom themes, use `source path/to/theme.cjsh` in your `~/.cjshrc` file or directly at the command line.

#### Configuration Directories
- `~/.cache/cjsh/` - Cache directory (history, exec cache)

#### Runtime Options
Multiple command-line flags are available to adjust startup behavior:

**General execution:**
- `--login` – Start in login mode (mirrors `-l`)
- `--interactive` – Force interactive behavior even when stdin is not a TTY
- `--debug` – Enable verbose startup diagnostics
- `--command=<command>` – Run a single command and exit
- `--version` – Print version information and exit
- `--help` – Display the built-in help text

**Feature toggles:**
- `--minimal` – Disable cjsh-specific features (themes, colors, completions, syntax highlighting,
  smart cd, sourcing, startup timers)
- `--no-themes` – Disable the theme system
- `--no-colors` – Disable color output
- `--no-titleline` – Skip dynamic title line updates
- `--no-source` – Skip sourcing `~/.cjshrc`
- `--no-completions` – Disable the completion system
- `--no-syntax-highlighting` – Disable syntax highlighting
- `--no-smart-cd` – Use the basic `cd` implementation
- `--no-prompt` – Use a minimal `#` prompt instead of the themed prompt
- `--no-history-expansion` – Turn off history expansion (`!!`, `!$`, etc.)
- `--show-startup-time` – Print how long startup took
- `--startup-test` – Enable startup test mode for diagnostics
- `--secure` – Run in secure mode with additional restrictions

See `cjsh --help` for complete list.

### Performance

#### Optimizations
- Compiled with aggressive optimization flags
- Theme caching to avoid repeated parsing
- Command lookup caching
- Prompt variable caching
- Executable path caching

#### Benchmarks
Theme rendering is typically 2-4x faster than popular alternatives like Starship and Powerlevel10k.

### Development Tools

#### Script Validation
- `syntax` command for checking script syntax
- `validate` command for command verification
- Detailed error messages with suggestions

#### History Management
- Persistent command history across sessions
- Configurable history size with `cjshopt set-history-max`
- Duplicate suppression (enabled by default)
- History search and editing
- Incremental search (Ctrl+R)
- History expansion (`!`, `!!`, `!$`, `!-1`, etc.)
- History file at `~/.cache/cjsh/history.txt`
- Exit codes persisted alongside each recorded command

See [Editing Guide](editing.md#history-management) for detailed history features.

#### Debugging Support
- Detailed error reporting
- Stack traces for script errors
- Command timing information

### Job Control

Full job control support:
- Background jobs with `&`
- Job listing with `jobs`
- Foreground/background switching with `fg`/`bg`
- Job waiting with `wait`
- Signal sending with `kill`
- Proper signal handling with `trap`

### Security

#### Secure Mode
When enabled with `--secure`:
- Restricts certain operations
- Prevents sourcing untrusted files
- Additional safety checks

#### Read-only Variables
Support for marking variables as read-only to prevent modification.

## Platform Support

### Supported Platforms
- Linux (all major distributions)
- macOS
- Windows with WSL (Windows Subsystem for Linux)
- Other *nix systems

### Requirements
- C++17 compatible compiler
- POSIX-compatible environment
- Terminal with color support (recommended)

## Installation Methods

### Package Manager (Recommended)

#### Homebrew (macOS/Linux)
```bash
brew tap CadenFinley/tap
brew install cjsh
```

#### Arch Linux (AUR)
```bash
# Using yay
yay -S cjsh

# Using paru
paru -S cjsh

# Or manually
git clone https://aur.archlinux.org/cjsh.git
cd cjsh
makepkg -si
```

### Manual Build
```bash
git clone https://github.com/CadenFinley/CJsShell.git
cd CJsShell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

See the Quick Start guide for detailed installation instructions.
