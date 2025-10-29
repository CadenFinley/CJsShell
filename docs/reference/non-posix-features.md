# POSIX+ Interactive Features

CJsShell targets approximately 95% POSIX coverage for scripting while layering modern conveniences and interactive features that extend beyond the formal specification. This document highlights the POSIX+ surface area, the experiences that are intentionally non-POSIX so you know what is different and how to control it.

## Interactive Enhancements (POSIX+)

### Advanced Prompt System

CJsShell includes a sophisticated theming and prompt system that is intentionally non-POSIX:

#### Custom Theme DSL
- **Feature**: Proprietary theme scripting language with `.cjsh` files
- **Location**: `themes/` directory, loaded via `source theme.cjsh`
- **POSIX Alternative**: Basic `PS1`, `PS2` environment variables
- **Why Non-POSIX**: POSIX only specifies basic prompt variables, not theming systems

```cjsh-theme
# Example theme syntax (POSIX+)
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

#### Highlighting Categories (POSIX+)
```bash
# Configure custom highlighting (POSIX+)
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

**Configuration Commands (POSIX+)**:
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
- **Visible Whitespace**: Optional markers for spaces while editing

**Configuration (POSIX+)**:
```bash
cjshopt multiline on|off              # Multi-line editing
cjshopt multiline-indent on|off       # Auto-indentation
cjshopt line-numbers on|off           # Line numbering
cjshopt visible-whitespace on|off     # Show whitespace markers
```

## Custom Built-in Commands (POSIX+)

### Enhanced Directory Navigation

#### Smart CD Command
When `--no-smart-cd` is not specified, CJsShell's `cd` includes:
- **Fuzzy Directory Matching**: Partial directory name completion
- **Directory Bookmarks**: Automatic bookmarking of frequently visited directories
- **Bookmark Management**: `cjshopt set-max-bookmarks <number>`
- **Blacklisting**: `cjshopt bookmark-blacklist add <path>`

```bash
# These features are POSIX+ (non-POSIX)
cd doc        # Matches "documents" directory
cd -          # Previous directory (POSIX compliant)
```

### History Expansion

CJsShell supports **bash-style history expansion** for interactive convenience:

#### Event Designators
- `!!` - Repeat the previous command
- `!n` - Repeat command number `n` from history
- `!-n` - Repeat the command `n` positions back
- `!string` - Repeat most recent command starting with `string`
- `!?string?` - Repeat most recent command containing `string`

#### Word Designators
- `!$` - Last argument of previous command
- `!^` - First argument of previous command (word 1)
- `!*` - All arguments of previous command (words 1-n)
- `!:n` - Argument `n` of previous command
- `!:n-m` - Arguments `n` through `m` of previous command
- `!:n-` - Arguments from `n` to the end

#### Quick Substitution
- `^old^new` - Replace `old` with `new` in previous command and execute

```bash
# Examples (POSIX+)
echo hello world
!!              # Repeats: echo hello world
echo !$         # Expands to: echo world
ls !^           # Uses first arg from previous command
^hello^goodbye  # Changes hello to goodbye and re-executes
!echo           # Runs most recent echo command
```

**Disabling**: Use `--no-history-expansion` flag or disable in minimal mode.  
**Auto-Disabled**: History expansion is automatically disabled in:
- Script mode (`cjsh script.sh`)
- Command mode (`cjsh -c "command"`)
- Piped input (preserves POSIX semantics)

### Configuration Command (`cjshopt`)

**POSIX+ (Non-POSIX)**: The `cjshopt` command provides shell configuration:

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

## Color and Visual Features (POSIX+)

### Advanced Color System

#### True Color Support
- **24-bit RGB Colors**: Full color spectrum support
- **Gradient Text**: Text with gradient color effects
- **Color Capability Detection**: Automatic terminal color detection
- **Color Environment Variables**: `COLORTERM`, `FORCE_COLOR` support

#### BBCode-style Color Markup
```bash
# POSIX+ color markup in themes
content "[color=#FF5555]Error[/color]"
content "[gradient(#FF0000,#00FF00)]Rainbow Text[/gradient]"
```

### Terminal Enhancements

#### Window Title Management
- **Dynamic Titles**: Automatic terminal title updates
- **Custom Title Formats**: Configurable title templates
- **Implementation**: Escape sequence injection

```cjsh-theme
# POSIX+ terminal title configuration
terminal_title "{PATH} - CJsShell"
```

## Configuration System (POSIX+)

### Custom Configuration Files

#### Startup Files
Beyond standard POSIX startup files:
- **`~/.cjprofile`**: Login configuration and startup flags
- **`~/.cjshrc`**: Interactive session configuration  
- **`~/.cjsh_logout`**: Optional logout script

**POSIX Equivalent**: `~/.profile`, `~/.sh_profile` (basic functionality only)

#### Runtime Configuration
- **`~/.cache/cjsh/`**: Cache directory for shell data
- **History Management**: Enhanced history with size limits
- **Bookmark Database**: Persistent directory bookmarks

### Command-Line Flags (POSIX+ Extensions)

```bash
# Shell customization flags (POSIX+)
cjsh --no-themes              # Disable theme system
cjsh --no-colors              # Disable color output  
cjsh --no-syntax-highlighting # Disable syntax highlighting
cjsh --no-completions         # Disable completion system
cjsh --no-smart-cd            # Use basic cd implementation
cjsh --minimal                # Minimal mode
cjsh --secure                 # Secure mode
```

## Performance Features (POSIX+)

### Dynamic PATH Resolution
- **Real-Time Executable Discovery**: Commands are discovered by crawling PATH on-demand
- **No Cache Staleness**: Always reflects current PATH state
- **Fast Lookups**: Optimized directory traversal for command resolution

## Extended Scripting Features

### Bash-like Extensions (POSIX+)
While maintaining POSIX compatibility, CJsShell supports some bash extensions:

#### Extended Test Expressions
```bash
# Bash-style extended tests (POSIX+)
[[ $var =~ regex ]]          # Pattern matching
[[ -v variable ]]            # Variable existence test
```

#### Process Substitution (Limited)
- **Note**: Marked as non-POSIX in test suite
- **Usage**: Limited support for `<(command)` syntax

### Here Strings (POSIX+)
```bash
# Here strings (bash extension, POSIX+)
command <<< "string"
```

## Compliance Notes

### POSIX Compliance Testing
CJsShell includes over 1500 POSIX-focused tests that verify:
- Standard shell scripting syntax
- Built-in command behavior
- Variable expansion
- Job control
- Signal handling

### Compatibility Modes
- **`-c` Flag**: Preserves POSIX semantics for script execution
- **Piped Input**: Automatic POSIX mode when shell receives piped input
- **Script Mode**: Non-interactive mode maintains strict POSIX behavior

### Disabling POSIX+ Features
Most POSIX+ features can be disabled for strict compatibility:

```bash
# Run in a standards-aligned mode
cjsh --minimal --no-themes --no-colors --no-syntax-highlighting \
     --no-completions --no-smart-cd
```

## Summary

CJsShell's POSIX+ features focus on enhancing the interactive shell experience while maintaining strong POSIX compatibility for scripting. The design philosophy allows users to benefit from modern shell conveniences in interactive use while ensuring scripts remain portable and standards-compliant.

**Key POSIX+ Areas**:
1. **Interactive Features**: Theming, syntax highlighting, advanced completion
2. **Enhanced Built-ins**: Smart cd, cjshopt configuration
3. **Visual Enhancements**: Colors, gradients, terminal title management
4. **Development Tools**: Syntax validation, command validation
5. **Configuration System**: Custom config files and runtime options

All POSIX+ features are designed to be non-intrusive to POSIX scripting behavior and can be disabled when strict standards adherence is required.