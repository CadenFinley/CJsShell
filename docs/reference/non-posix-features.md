# Non-POSIX Features in CJsShell

CJsShell aims for approximately 95% POSIX compliance while adding modern conveniences and interactive features that extend beyond the POSIX shell specification. This document outlines the features and behaviors that deviate from strict POSIX compliance.

## Interactive Features (Non-POSIX)

### Advanced Prompt System

CJsShell includes a sophisticated theming and prompt system that is completely non-POSIX:

#### Custom Theme DSL
- **Feature**: Proprietary theme scripting language with `.cjsh` files
- **Location**: `themes/` directory, loaded via `source theme.cjsh`
- **POSIX Alternative**: Basic `PS1`, `PS2` environment variables
- **Why Non-POSIX**: POSIX only specifies basic prompt variables, not theming systems

```cjsh-theme
# Example theme syntax (non-POSIX)
theme_definition {
  ps1 {
    segment "username" {
      content "{USERNAME}@{HOSTNAME}:"
      fg "#5555FF"
    }
  }
}
```

#### Advanced Prompt Variables
CJsShell provides extensive prompt variables beyond POSIX:
- **Git Integration**: `{GIT_BRANCH}`, `{GIT_STATUS}`, `{GIT_AHEAD}`, `{GIT_BEHIND}`
- **System Info**: `{CPU_USAGE}`, `{MEMORY_USAGE}`, `{DISK_USAGE}`
- **Language Detection**: `{PYTHON_VERSION}`, `{NODE_VERSION}`, `{RUST_VERSION}`
- **Container Detection**: `{DOCKER_CONTEXT}`, `{PODMAN_CONTEXT}`
- **Network Info**: `{IP_ADDRESS}`, `{WIFI_SSID}`

**POSIX Equivalent**: Only `PS1`, `PS2`, `PS4` with basic variables like `$PWD`, `$USER`

### Syntax Highlighting

#### Real-time Highlighting
- **Feature**: Live syntax highlighting as you type
- **Implementation**: Custom token classification and ANSI color application
- **POSIX Alternative**: None (plain text input only)
- **Configuration**: `cjshopt style_def <token_type> <style>`

#### Highlighting Categories (Non-POSIX)
```bash
# Configure custom highlighting (non-POSIX)
cjshopt style_def "unknown-command" "bold color=#FF5555"
cjshopt style_def "builtin" "color=#FFB86C"
cjshopt style_def "keyword" "bold color=#BD93F9"
```

### Tab Completion System

#### Advanced Completion Features
- **Fuzzy Matching**: Approximate string matching for commands and files
- **Context-Aware Suggestions**: Different completions based on command context
- **Spell Correction**: Automatic correction of typos
- **Preview System**: Shows completion results before selection

**Configuration Commands (Non-POSIX)**:
```bash
cjshopt completion-case on|off        # Case sensitivity
cjshopt completion-spell on|off       # Spell correction
cjshopt completion-preview on|off     # Preview system
```

**POSIX Alternative**: Basic filename completion only

### Interactive Line Editing

#### Enhanced Text Editing
- **Multi-line Input**: Automatic line continuation with intelligent indentation
- **Bracket Matching**: Automatic bracket, quote, and parentheses matching
- **Smart Indentation**: Context-aware indentation in multi-line constructs
- **Line Numbers**: Optional line numbering in multi-line mode

**Configuration (Non-POSIX)**:
```bash
cjshopt multiline on|off              # Multi-line editing
cjshopt multiline-indent on|off       # Auto-indentation
cjshopt line-numbers on|off           # Line numbering
```

## Custom Built-in Commands (Non-POSIX)

### Enhanced Directory Navigation

#### Smart CD Command
When `--no-smart-cd` is not specified, CJsShell's `cd` includes:
- **Fuzzy Directory Matching**: Partial directory name completion
- **Directory Bookmarks**: Automatic bookmarking of frequently visited directories
- **Bookmark Management**: `cjshopt set-max-bookmarks <number>`
- **Blacklisting**: `cjshopt bookmark-blacklist add <path>`

```bash
# These features are non-POSIX
cd doc        # Matches "documents" directory
cd -          # Previous directory (POSIX compliant)
```

#### Enhanced LS Command
When `--disable-custom-ls` is not specified:
- **Color-coded Output**: File type and permission coloring
- **Enhanced Formatting**: Improved visual presentation
- **Permission Highlighting**: Color-coded permission display
- **Extended Attributes**: Support for extended file attributes

### Configuration Command (`cjshopt`)

**Entirely Non-POSIX**: The `cjshopt` command provides shell configuration:

```bash
cjshopt style_def <token> <style>     # Syntax highlighting styles
cjshopt keybind <action> <key>        # Custom key bindings
cjshopt generate-profile              # Generate ~/.cjprofile
cjshopt generate-rc                   # Generate ~/.cjshrc
cjshopt set-history-max <number>      # History configuration
```

### Development Tools

#### Syntax Validation (`syntax`)
- **Feature**: Script syntax checking without execution
- **Usage**: `syntax script.sh`
- **POSIX Alternative**: None (must attempt execution)

#### Command Validation (`validate`)
- **Feature**: Verify command existence and syntax
- **Usage**: `validate command_name`
- **POSIX Alternative**: `command -v` (basic existence check only)

## Color and Visual Features (Non-POSIX)

### Advanced Color System

#### True Color Support
- **24-bit RGB Colors**: Full color spectrum support
- **Gradient Text**: Text with gradient color effects
- **Color Capability Detection**: Automatic terminal color detection
- **Color Environment Variables**: `COLORTERM`, `FORCE_COLOR` support

#### BBCode-style Color Markup
```bash
# Non-POSIX color markup in themes
content "[color=#FF5555]Error[/color]"
content "[gradient(#FF0000,#00FF00)]Rainbow Text[/gradient]"
```

### Terminal Enhancements

#### Window Title Management
- **Dynamic Titles**: Automatic terminal title updates
- **Custom Title Formats**: Configurable title templates
- **Implementation**: Escape sequence injection

```cjsh-theme
# Non-POSIX terminal title configuration
terminal_title "{PATH} - CJsShell"
```

## Configuration System (Non-POSIX)

### Custom Configuration Files

#### Startup Files
Beyond standard POSIX startup files:
- **`~/.cjprofile`**: Login configuration and startup flags
- **`~/.cjshrc`**: Interactive session configuration  
- **`~/.cjsh_logout`**: Optional logout script

**POSIX Equivalent**: `~/.profile`, `~/.sh_profile` (basic functionality only)

#### Runtime Configuration
- **`~/.cache/cjsh/`**: Cache directory for performance optimization
- **Executable Cache**: Cached PATH command lookups
- **History Management**: Enhanced history with size limits
- **Bookmark Database**: Persistent directory bookmarks

### Command-Line Flags (Non-POSIX Extensions)

```bash
# Shell customization flags (non-POSIX)
cjsh --no-themes              # Disable theme system
cjsh --no-colors              # Disable color output  
cjsh --no-syntax-highlighting # Disable syntax highlighting
cjsh --no-completions         # Disable completion system
cjsh --no-smart-cd            # Use basic cd implementation
cjsh --disable-custom-ls      # Use system ls command
cjsh --minimal                # Minimal mode
cjsh --secure                 # Secure mode
```

## Performance Features (Non-POSIX)

### Caching Systems
- **Command Lookup Cache**: Cached executable path resolution
- **Theme Cache**: Parsed theme caching for performance
- **History Optimization**: Efficient history file management
- **Variable Caching**: Prompt variable caching

### Background Processing
- **Asynchronous Operations**: Non-blocking operations where possible
- **Lazy Loading**: Deferred initialization of non-essential features

## Extended Scripting Features

### Bash-like Extensions (Non-POSIX)
While maintaining POSIX compatibility, CJsShell supports some bash extensions:

#### Extended Test Expressions
```bash
# Bash-style extended tests (non-POSIX)
[[ $var =~ regex ]]          # Pattern matching
[[ -v variable ]]            # Variable existence test
```

#### Process Substitution (Limited)
- **Note**: Marked as non-POSIX in test suite
- **Usage**: Limited support for `<(command)` syntax

### Here Strings (Non-POSIX)
```bash
# Here strings (bash extension, non-POSIX)
command <<< "string"
```

## Compliance Notes

### POSIX Compliance Testing
CJsShell includes over 1000 POSIX compliance tests that verify:
- Standard shell scripting syntax
- Built-in command behavior
- Variable expansion
- Job control
- Signal handling

### Compatibility Modes
- **`-c` Flag**: Ensures POSIX compliance for script execution
- **Piped Input**: Automatic POSIX mode when shell receives piped input
- **Script Mode**: Non-interactive mode maintains strict POSIX behavior

### Disabling Non-POSIX Features
Most non-POSIX features can be disabled for strict compatibility:

```bash
# Achieve near-POSIX compliance
cjsh --minimal --no-themes --no-colors --no-syntax-highlighting \
     --no-completions --no-smart-cd --disable-custom-ls
```

## Summary

CJsShell's non-POSIX features focus on enhancing the interactive shell experience while maintaining strong POSIX compatibility for scripting. The design philosophy allows users to benefit from modern shell conveniences in interactive use while ensuring scripts remain portable and standards-compliant.

**Key Non-POSIX Areas**:
1. **Interactive Features**: Theming, syntax highlighting, advanced completion
2. **Enhanced Built-ins**: Smart cd, custom ls, cjshopt configuration
3. **Visual Enhancements**: Colors, gradients, terminal title management
4. **Development Tools**: Syntax validation, command validation
5. **Configuration System**: Custom config files and runtime options

All non-POSIX features are designed to be non-intrusive to POSIX compliance and can be disabled when strict standards adherence is required.