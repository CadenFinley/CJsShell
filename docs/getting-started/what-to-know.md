# What You Need to Know

Welcome to cjsh! This guide provides a comprehensive overview of the features available in your shell environment. cjsh comes with a rich set of features enabled by default, most of which can be configured or disabled to suit your workflow preferences.

## Default Feature Set

All features listed below are **enabled by default** unless otherwise specified. Configuration options are provided where applicable.

---

## Visual & Interface Features

### Themes
**Status:** Enabled  
**Configuration:** `cjshopt set theme <theme-name>`  
**Disable:** `cjshopt set theme default`

Apply custom color schemes and prompt styles to personalize your shell experience. Themes are located in the `themes/` directory and can be customized or created from scratch.

### True Color Support
**Status:** Enabled (if terminal supports it)  
**Configuration:** Auto-detected based on terminal capabilities  
**Disable:** `cjshopt set truecolor off`

Enables 24-bit RGB color support for enhanced visual rendering of prompts, syntax highlighting, and themes.

### Syntax Highlighting
**Status:** Enabled  
**Configuration:** `cjshopt set syntax-highlighting <on|off>`  
**Disable:** `cjshopt set syntax-highlighting off`

Real-time syntax highlighting for commands, arguments, paths, and strings as you type. Helps identify errors before execution.

---

## Completion & Input Features

### Fuzzy Completions
**Status:** Enabled  
**Configuration:** `cjshopt set fuzzy-completions <on|off>`  
**Disable:** `cjshopt set fuzzy-completions off`

Intelligent command and path completion that matches partial strings even when characters are non-contiguous (e.g., "gco" matches "git checkout").

### Completion Preview Inline
**Status:** Enabled  
**Configuration:** Built-in with fuzzy completions  
**Disable:** Disabled automatically when fuzzy completions are off

Displays completion suggestions inline as you type, allowing you to preview and accept suggestions with minimal keystrokes.

### Tab-Enabled Spell Corrections
**Status:** Enabled  
**Configuration:** `cjshopt set spell-correct <on|off>`  
**Disable:** `cjshopt set spell-correct off`

Automatically suggests corrections for misspelled commands and paths. Press Tab to accept the correction.

### Case-Insensitive Completion
**Status:** Enabled  
**Configuration:** `cjshopt set case-sensitive <on|off>`  
**Disable:** `cjshopt set case-sensitive on`

Allows completions and matching to ignore case distinctions, making command input more forgiving.

### Multi-Line Editing
**Status:** Enabled  
**Configuration:** Built-in editor feature  
**Disable:** Cannot be disabled

Edit complex, multi-line commands with full cursor navigation and editing capabilities across lines.

---

## Navigation Features

### Smart CD
**Status:** Enabled  
**Configuration:** `cjshopt set smart-cd <on|off>`  
**Disable:** `cjshopt set smart-cd off`

Intelligent directory navigation that performs fuzzy matching and learns your frequently accessed directories.

### Auto CD
**Status:** Enabled  
**Configuration:** `cjshopt set auto-cd <on|off>`  
**Disable:** `cjshopt set auto-cd off`

Change directories by typing just the path name, without needing the `cd` command (e.g., typing `/usr/local` changes to that directory).

### Custom Dynamic LS
**Status:** Enabled  
**Configuration:** Customizable through cjshrc  
**Disable:** `cjshopt set auto-ls off`

Automatically displays directory contents after changing directories, with customizable formatting and color coding.

---

## History Features

### History Expansions
**Status:** Enabled  
**Configuration:** `cjshopt set history-expansion <on|off>`  
**Disable:** `cjshopt set history-expansion off`

Use bash-style history expansions like `!!` (last command), `!$` (last argument), `!n` (command number n), and more.

### Atuin-Style History Search
**Status:** Enabled  
**Configuration:** Press Ctrl+R to activate  
**Disable:** `cjshopt set history-search <on|off>`

Interactive, searchable command history with fuzzy matching, timestamps, and execution context.

### Multi-Session Live-Synced History
**Status:** Enabled  
**Configuration:** `cjshopt set shared-history <on|off>`  
**Disable:** `cjshopt set shared-history off`

Command history is synchronized in real-time across all active shell sessions, ensuring consistent history access.

---

## Scripting & Compatibility Features

### .cjshrc Sourcing
**Status:** Enabled  
**Configuration:** Automatic on shell startup  
**File Location:** `~/.cjshrc`

Configuration file sourced at startup, allowing you to set aliases, functions, environment variables, and shell options.

### .cjprofile and Login Startup Arguments
**Status:** Enabled for login shells  
**Configuration:** `cjshopt login-startup-arg <argument>`  
**File Location:** `~/.cjprofile`

The `.cjprofile` file is sourced for login shells and can be used to set environment variables and perform one-time initialization. Use `cjshopt login-startup-arg` to add additional startup flags that will be automatically included in your `.cjprofile` file.

**Example:**
```bash
# Add a custom startup flag to .cjprofile
cjshopt login-startup-arg "--verbose"

# Add multiple flags
cjshopt login-startup-arg "--no-colors --fast-start"
```

This feature is particularly useful for configuring shell behavior that should only apply to login sessions, such as setting up environment variables, initializing terminal settings, or loading specific modules.

### POSIX Compliance
**Status:** Enabled  
**Configuration:** High compatibility mode by default  
**Note:** Largely POSIX-compliant with shell scripting

Scripts written for POSIX-compliant shells should work with minimal or no modifications.

### Bash-Style Features
**Status:** Enabled  
**Configuration:** Various individual options  
**Features Include:**
- `[[]]` conditional expressions
- Globbing patterns (`*`, `?`, `[]`)
- Brace expansion
- Process substitution
- Redirection operators (including clobbering)

### Advanced Syntax Validation
**Status:** Enabled  
**Configuration:** Always active  
**Disable:** Cannot be disabled

Real-time syntax checking with detailed error reporting, helping catch issues before command execution.

---

## Extended Features

### Abbreviations
**Status:** Enabled  
**Configuration:** `abbr add <shortcut> <expansion>`  
**Disable:** N/A (feature is opt-in per abbreviation)  
**Example:** `abbr add gs "git status"`

Define short aliases that expand to longer commands when you press Space, providing visual feedback during typing.

### Typeahead
**Status:** Enabled  
**Configuration:** Automatic buffering system  
**Disable:** Cannot be disabled

Buffers keystrokes while the shell is processing, ensuring no input is lost during command execution or slow operations.

### Emacs-Style Keybindings
**Status:** Enabled  
**Configuration:** `cjshopt set keymap <emacs|vi>`  
**Disable:** Switch to vi mode: `cjshopt set keymap vi`

Standard Emacs keybindings for command-line editing (Ctrl+A, Ctrl+E, Ctrl+K, etc.).

### ZLE-Style Widgets
**Status:** Enabled  
**Configuration:** Customizable through cjshrc  
**Disable:** Individual widgets can be unbound

Zsh-like line editor widgets that allow custom keybindings and command-line manipulation functions.

---

## Getting Help

For more information on any feature:
- Use `cjshopt help` to see all available options
- Use `help <builtin>` for built-in command documentation
- See the full documentation at `docs/reference/`
- Visit configuration examples in `.cjshrc` templates

## Quick Configuration Examples

```bash
# Disable syntax highlighting
cjshopt set syntax-highlighting off

# Switch to vi keybindings
cjshopt set keymap vi

# Disable auto-cd
cjshopt set auto-cd off

# Turn off fuzzy completions
cjshopt set fuzzy-completions off
```

For a complete list of configuration options, run `cjshopt list`.


