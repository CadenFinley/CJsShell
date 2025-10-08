# Features Overview

CJ's Shell is designed to provide a powerful, fast, and customizable shell experience with minimal configuration required.

## Core Features

### POSIX Compliance
CJ's Shell aims for approximately 95% POSIX compliance, making it compatible with most existing shell scripts while adding modern conveniences.

- Standard POSIX shell scripting syntax
- Compatible with most sh/bash scripts
- Comprehensive test suite with over 1000 POSIX compliance tests

**See Also**: [Non-POSIX Features](non-posix-features.md) for detailed documentation of features that extend beyond POSIX compliance.

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

#### Line Editing
CJ's Shell embeds the isocline line editor for advanced text editing capabilities:

- Multiline input support
- Syntax highlighting
- Context-aware tab completion
- Incremental history search (Ctrl+R)
- Readline-style shortcuts
- Interactive key binding cheat sheet (F1)

#### Smart Directory Navigation
Enhanced `cd` command with intelligent features:

- Fuzzy directory matching
- Directory bookmarks
- Adjustable bookmark limit with `cjshopt set-max-bookmarks`
- Blacklist unwanted locations via `cjshopt bookmark-blacklist`
- Previous directory switching with `cd -`
- Can be disabled with `--no-smart-cd` flag

#### Enhanced ls Command
Custom `ls` implementation with improved formatting:

- Color-coded file types
- Better visual presentation
- Respects terminal capabilities
- Can be disabled with `--disable-custom-ls` flag

#### Auto-completion
Built-in completion system with:

- Command completion from PATH
- File and directory completion
- Fuzzy matching support
- Context-aware suggestions
- Configurable case sensitivity

#### Syntax Highlighting
Real-time syntax highlighting as you type:

- Command recognition
- String highlighting
- Operator highlighting
- Customizable color schemes via `cjshopt style_def`

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
Multiple command-line flags for customizing behavior:

- `--no-themes` - Disable theme system
- `--no-colors` - Disable color output
- `--no-source` - Skip sourcing ~/.cjshrc
- `--no-completions` - Disable completion system
- `--no-syntax-highlighting` - Disable syntax highlighting
- `--no-smart-cd` - Use basic cd implementation
- `--disable-custom-ls` - Use system ls command
- `--minimal` - Minimal mode (disables most features)
- `--secure` - Secure mode (restricted operations)

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
- Persistent command history
- Configurable history size with `cjshopt set-history-max`
- History search and editing

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
```bash
brew tap CadenFinley/tap
brew install cjsh
```

Works on macOS and Linux (via Linuxbrew).

### Manual Build
```bash
git clone https://github.com/CadenFinley/CJsShell.git
cd CJsShell
./toolchain/build.sh
```

See the Quick Start guide for detailed installation instructions.
