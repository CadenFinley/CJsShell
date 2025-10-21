# Interactive Line Editing

CJ's Shell uses the [isocline](https://github.com/daanx/isocline) line editor library to provide a powerful and feature-rich interactive editing experience. This document details all editing features currently available in cjsh.

## Overview

Isocline is a modern, pure C line editing library that provides advanced terminal interaction capabilities. CJ's Shell leverages and extends isocline to offer:

- Multiline editing with intelligent indentation
- Real-time syntax highlighting
- Context-aware tab completion
- Inline hints and preview suggestions
- Customizable key bindings
- Line numbering for multiline input
- Optional visible markers for whitespace characters
- History search and management
- Brace matching and auto-insertion
- Spell correction
- Fish-style abbreviations (automatic expansion on word boundaries)

All of these features can be configured through the `cjshopt` command or in your `~/.cjshrc` configuration file.

## Core Editing Features

### Multiline Input

CJ's Shell supports seamless multiline input for complex commands, heredocs, and incomplete statements.

**Features:**
- Automatic continuation when lines are incomplete
- Smart indentation that aligns with the initial prompt
- Line numbers for easy navigation
- Both absolute and relative line numbering modes

**Configuration:**
```bash
# Enable/disable multiline input (enabled by default)
cjshopt multiline on|off|status

# Enable/disable automatic indentation (enabled by default)
cjshopt multiline-indent on|off|status
```

**Multiline Detection:**
CJ's Shell automatically enters multiline mode when:
- A line ends with a backslash (`\`)
- Quotes are unclosed (`"`, `'`, or backticks)
- Control structures are incomplete (`if`, `while`, `for`, `case`, etc.)
- Heredocs are being entered (`<<`, `<<-`)
- Brackets/braces/parentheses are unclosed

### Line Numbers

When in multiline mode, line numbers help track your position in the input.

**Modes:**
- **Absolute numbering**: Shows actual line numbers (1, 2, 3, ...)
- **Relative numbering**: Shows distance from current line (0 for current, ±N for others)

**Configuration:**
```bash
# Enable absolute line numbers (default)
cjshopt line-numbers on
cjshopt line-numbers absolute

# Enable relative line numbers
cjshopt line-numbers relative

# Disable line numbers
cjshopt line-numbers off

# Check current status
cjshopt line-numbers status
```

**Current Line Highlighting:**
The line containing the cursor can be highlighted differently:
```bash
# Enable/disable current line number highlighting (enabled by default)
cjshopt current-line-number-highlight on|off|status
```

The line number styles can be customized:
- `ic-linenumbers`: Style for regular line numbers
- `ic-linenumber-current`: Style for the current line number

### Visible Whitespace Markers

You can visualize whitespace characters while editing to spot stray spaces or indentation issues. When enabled, spaces are rendered using a subtle middle-dot marker.

**Configuration:**
```bash
# Show or hide visible whitespace markers (disabled by default)
cjshopt visible-whitespace on|off|status
```

Pair this option with custom styling via `cjshopt style_def ic-whitespace-char "<style>"` to adjust the marker color.

### Syntax Highlighting

Real-time syntax highlighting provides visual feedback as you type.

**Highlighted Elements:**
- Commands (builtins, executables, aliases)
- Keywords (`if`, `then`, `else`, `while`, `for`, etc.)
- Strings (single and double quoted)
- Variables and parameter expansions
- Operators (pipes, redirections, logical operators)
- Comments
- Errors (unmatched quotes, invalid syntax)

**Configuration:**
```bash
# Enable/disable syntax highlighting (enabled by default)
# Note: Can also be controlled with --no-syntax-highlighting flag

# Customize highlighting styles
cjshopt style_def <token_type> <style>
```

**Available Style Names:**
- `ic-keyword`: Shell keywords
- `ic-command`: Valid commands
- `ic-error`: Invalid commands or syntax errors
- `ic-string`: String literals
- `ic-comment`: Comments
- `ic-operator`: Operators
- `ic-variable`: Variables
- `ic-number`: Numeric literals
- `ic-bracket`: Brackets and braces
- `ic-bracket-match`: Matching bracket pairs

**Syntax Highlighting Control:**
The syntax highlighter can be temporarily disabled with the `--no-syntax-highlighting` startup flag.

### Completion System

CJ's Shell features a sophisticated completion system that provides context-aware suggestions.

**Completion Types:**
- **Command completion**: Completes commands from PATH, builtins, aliases, and functions
- **File/directory completion**: Completes paths with proper quoting and escaping
- **Variable completion**: Completes shell variable names
- **User completion**: Completes usernames (after `~`)
- **Hostname completion**: Completes hostnames (for ssh, scp, etc.)

**Features:**
- Fuzzy matching for typo tolerance
- Frequency-based ranking (commonly used completions appear first)
- Source attribution (shows where completions come from)
- Preview of selected completion
- Automatic expansion with auto-tab

**Configuration:**
```bash
# Enable/disable completion preview (enabled by default)
cjshopt completion-preview on|off|status

# Enable/disable auto-tab (disabled by default)
# Auto-tab automatically completes unique prefixes
cjshopt auto-tab on|off|status

# Configure case sensitivity (enabled by default)
cjshopt completion-case on|off|status

# Enable/disable spell correction (enabled by default)
cjshopt completion-spell on|off|status
```

**Using Completions:**
- Press `Tab` to show completions
- Press `Tab` again to cycle through options
- Use arrow keys to navigate the completion menu
- Press `Enter` to accept a completion
- Press `Esc` to cancel

### Hints and Inline Help

CJ's Shell provides inline hints and help to assist with command input.

**Hints:**
When there's a single possible completion, a hint is displayed inline with dimmed text.

**Features:**
- Shows the rest of the word being typed
- Appears after a configurable delay
- Can be accepted by pressing → (right arrow)

**Configuration:**
```bash
# Enable/disable hints (enabled by default)
cjshopt hint on|off|status

# Set hint delay in milliseconds (0ms by default)
cjshopt hint-delay <milliseconds>
```

**Inline Help:**
Short help messages are displayed for certain operations:
- History search instructions
- Completion menu navigation
- Special key binding hints

**Configuration:**
```bash
# Enable/disable inline help (enabled by default)
cjshopt inline-help on|off|status
```

**Full Help:**
Press `F1` at any time to display the complete key binding cheat sheet, regardless of the inline-help setting.

### Fish-Style Abbreviations

CJ's Shell supports fish-style abbreviations that expand typed shortcuts into longer phrases as soon as you type a whitespace character or submit the line. This is powered directly by the isocline editor, so expansions happen inline without disrupting your cursor position or undo history.

**Usage:**
- Define abbreviations with the `abbr` builtin: `abbr gs='git status --short --branch'`
- Remove them with `unabbr`: `unabbr gs`
- List all active abbreviations by running `abbr` with no arguments

**Behavior:**
- Triggers expand when followed by a space, tab, newline, or when you press `Enter`
- Expansion text is inserted in place of the trigger, preserving subsequent input
- Abbreviations live in the current shell session and persist across reads within that session

**Defaults:**
- `abbr` → `abbreviate`
- `unabbr` → `unabbreviate`

Add `abbr` definitions to your `~/.cjshrc` to load them automatically on startup.

### Brace Matching and Auto-Insertion

Visual feedback for matching brackets, braces, and parentheses.

**Brace Matching:**
When the cursor is next to a brace, its matching pair is highlighted. Unmatched braces are highlighted as errors.

**Default Brace Pairs:**
- Parentheses: `()`
- Square brackets: `[]`
- Curly braces: `{}`

**Auto-Insertion:**
When you type an opening brace, the closing brace is automatically inserted (enabled by default).

**Default Auto-Insertion Pairs:**
- Parentheses: `()`
- Square brackets: `[]`
- Curly braces: `{}`
- Double quotes: `""`
- Single quotes: `''`

**Note:** Brace matching and auto-insertion are currently controlled by isocline's default settings and cannot be configured via `cjshopt`, but the feature is active.

## History Management

### History Features

CJ's Shell maintains a persistent command history with advanced search capabilities.

**Features:**
- Persistent history across sessions
- Configurable maximum entries
- Duplicate suppression
- History expansion (`!`, `!!`, `!$`, etc.)
- Incremental search

**Configuration:**
```bash
# Set maximum history entries
cjshopt set-history-max <number>

# Use default history size
cjshopt set-history-max default

# Check current setting
cjshopt set-history-max status
```

**History File:**
History is stored at `~/.cache/cjsh/history.txt`

**Duplicate Handling:**
By default, duplicate entries are not stored in history to keep it clean and relevant.

### History Search

**Incremental Search (Ctrl+R):**
1. Press `Ctrl+R` to enter search mode
2. Type to search through history
3. Press `Ctrl+R` again to find the next match
4. Press `Enter` to execute the found command
5. Press `Esc` to cancel search

**History Navigation:**
- `↑` (Up Arrow): Previous command
- `↓` (Down Arrow): Next command
- `Alt+<`: Jump to oldest history entry
- `Alt+>`: Jump to newest history entry

## Key Bindings

CJ's Shell supports customizable key bindings with multiple profiles.

### Key Binding Profiles

**Available Profiles:**
- `emacs`: Emacs-style key bindings (default)
- `vi`: Vi/Vim-style key bindings

**Set Profile:**
```bash
cjshopt keybind profile set emacs|vi
```

### Common Key Bindings (Emacs Mode)

#### Cursor Movement
- `Ctrl+A`: Move to beginning of line
- `Ctrl+E`: Move to end of line
- `Ctrl+F` / `→`: Move forward one character
- `Ctrl+B` / `←`: Move backward one character
- `Alt+F`: Move forward one word
- `Alt+B`: Move backward one word
- `↑`: Previous line (or previous history)
- `↓`: Next line (or next history)

#### Editing
- `Ctrl+D`: Delete character under cursor (or exit if line is empty)
- `Ctrl+H` / `Backspace`: Delete character before cursor
- `Ctrl+W`: Delete word before cursor
- `Alt+D`: Delete word after cursor
- `Ctrl+U`: Delete from cursor to beginning of line
- `Ctrl+K`: Delete from cursor to end of line
- `Ctrl+Y`: Paste (yank) last deleted text
- `Ctrl+T`: Transpose characters
- `Alt+T`: Transpose words

#### History
- `Ctrl+R`: Incremental history search (reverse)
- `Ctrl+S`: Incremental history search (forward)
- `↑`: Previous history entry
- `↓`: Next history entry
- `Alt+<`: First history entry
- `Alt+>`: Last history entry

#### Completion
- `Tab`: Trigger completion / cycle through completions
- `Shift+Tab`: Cycle backward through completions

#### Special
- `Ctrl+C`: Cancel current input
- `Ctrl+D`: Exit shell (when buffer is empty)
- `Ctrl+L`: Clear screen
- `Enter`: Execute command
- `F1`: Show help / key binding cheat sheet
- `Esc`: Cancel operation

### Custom Key Bindings

You can inspect and customize key bindings through the `cjshopt keybind` subcommands:

```bash
# List all current key bindings (safe at runtime)
cjshopt keybind list

# Show available key binding profiles
cjshopt keybind profile list

# Activate a key binding profile (add to ~/.cjshrc to persist)
cjshopt keybind profile set vi

# Replace bindings for an action (config file only)
cjshopt keybind set cursor-left ctrl-h|left

# Add bindings without removing existing ones (config file only)
cjshopt keybind add delete-word-end alt-d

# Remove specific key bindings (config file only)
cjshopt keybind clear ctrl-h ctrl-b

# Remove all custom bindings for an action (config file only)
cjshopt keybind clear-action delete-word-end

# Restore default bindings (config file only)
cjshopt keybind reset
```

**Key Specification Format:**
- `ctrl-<key>`: Control key combinations (e.g., `ctrl-a`)
- `alt-<key>`: Alt/Meta key combinations (e.g., `alt-f`)
- `shift-<key>`: Shift key combinations (e.g., `shift-tab`)
- `f<N>`: Function keys (e.g., `f1`, `f12`)
- `<key>`: Regular keys (e.g., `tab`, `enter`)

## Command-Driven Key Bindings

Use the extended key binding namespace to trigger shell commands directly from key presses:

```bash
# List custom command key bindings (safe at runtime)
cjshopt keybind ext list

# Bind Ctrl+G to run a command (add to ~/.cjshrc)
cjshopt keybind ext set ctrl-g 'cjsh-widget accept'

# Remove a command binding
cjshopt keybind ext clear ctrl-g

# Clear every custom command binding
cjshopt keybind ext reset
```

Command key bindings typically leverage the `cjsh-widget` builtin to read or modify editor state. As
with standard bindings, modifications must be placed in configuration files; inspection commands can
run interactively, but setters only take effect during startup.

## Prompt Customization

### Prompt Cleanup

Control how the prompt is displayed after accepting input.

**Features:**
- Removes full prompt and shows only trailing segment
- Optional empty line insertion
- Multiline truncation

**Configuration:**
CJ's Shell automatically manages prompt cleanup based on your theme's settings. Theme authors can control:
- `cleanup_removes_prompt()`: Whether to enable prompt cleanup
- `cleanup_adds_empty_line()`: Whether to add an empty line after
- `cleanup_truncates_multiline()`: Whether to truncate multiline input

### Prompt Markers

The prompt system uses markers to indicate different states:

**Primary Prompt Marker:**
Displayed at the end of the main prompt (default: `"> "`).

**Continuation Prompt Marker:**
Displayed for continuation lines in multiline input.

**Customization:**
These are typically set by your theme and control the visual appearance of prompts.

## Terminal Features

### Color Support

CJ's Shell automatically detects terminal capabilities:

**Color Depth Detection:**
- Monochrome (1-bit)
- 8 colors (3-bit) with bold for bright
- 16 colors (4-bit)
- 256 colors (8-bit)
- True color (24-bit RGB)

**Environment Variables:**
- `COLORTERM`: Indicates color capability (`truecolor`, `24bit`, `256color`, etc.)

**Disable Colors:**
Use the `--no-colors` flag or set `TERM=dumb` to disable all color output.

### BBCode Formatting

CJ's Shell supports BBCode-style formatting in prompts and output:

**Tags:**
- `[b]...[/b]`: Bold text
- `[i]...[/i]`: Italic text
- `[u]...[/u]`: Underline text
- `[r]...[/r]`: Reverse video
- `[color]...[/color]`: Color names (e.g., `[red]`, `[blue]`)
- `[#RRGGBB]...[/]`: Hex RGB colors
- `[on color]...[/]`: Background colors

**Style Combinations:**
```
[b red on blue]Bold red text on blue background[/]
[i #ff00ff]Italic magenta text[/]
```

## Advanced Features

### Spell Correction

Automatic spell correction for commands and completions.

**How It Works:**
When no exact match is found, cjsh attempts to find the closest match using edit distance algorithms.

**Configuration:**
```bash
# Enable/disable spell correction (enabled by default)
cjshopt completion-spell on|off|status
```

### Heredoc Support

Full editing capabilities when entering heredocs:

```bash
cat <<EOF
# You get full editing features here:
# - Syntax highlighting
# - Multiline editing
# - History access
# - All normal key bindings
EOF
```

**Features:**
- Each line is edited with full isocline capabilities
- Supports `<<-` (strip leading tabs)
- Delimiter detection ends input
- Ctrl+C/Ctrl+D cancels heredoc input

### Readline Integration

CJ's Shell uses isocline's unified `ic_readline()` API for input, which provides:

**Inline Right Text:**
Some prompts may display additional information on the right side of the input line (configured by themes).

**Initial Input:**
Commands can pre-populate the input buffer (used by `fc` command for editing).

**Special Return Values:**
- `IC_READLINE_TOKEN_CTRL_C`: Returned when Ctrl+C is pressed on empty buffer
- `IC_READLINE_TOKEN_CTRL_D`: Returned when Ctrl+D is pressed on empty buffer (EOF)

### Non-TTY Mode

When standard input is not a terminal (pipe, file redirect, debugger, etc.):
- Line editing is disabled
- Input is read directly from the stream
- Prompts are still displayed but without styling
- Scripts and pipes work seamlessly

## Style Definitions

All visual aspects of the editor can be customized through style definitions.

### Available Styles

**Prompt Styles:**
- `ic-prompt`: Main prompt text
- `ic-linenumbers`: Line numbers in multiline mode
- `ic-linenumber-current`: Current line number highlight

**Syntax Highlighting Styles:**
- `ic-keyword`: Shell keywords
- `ic-command`: Valid commands
- `ic-error`: Errors and invalid commands
- `ic-string`: String literals
- `ic-comment`: Comments
- `ic-operator`: Operators
- `ic-variable`: Variables
- `ic-number`: Numbers
- `ic-bracket`: Brackets
- `ic-bracket-match`: Matching brackets

**Editor Styles:**
- `ic-hint`: Inline hints
- `ic-selection`: Selected text
- `ic-completion`: Completion menu
- `ic-completion-selected`: Selected completion

### Style Syntax

```bash
cjshopt style_def <style-name> "<bbcode-style>"
```

**Examples:**
```bash
# Make errors bright red and bold
cjshopt style_def ic-error "bold red"

# Use custom RGB color for commands
cjshopt style_def ic-command "#00ff00"

# Style keywords with italic blue
cjshopt style_def ic-keyword "italic blue"

# Combine multiple attributes
cjshopt style_def ic-string "italic #ffaa00"
```

**Style Attributes:**
- Colors: `red`, `blue`, `green`, `yellow`, `cyan`, `magenta`, `white`, `black`
- ANSI colors: `ansi-red`, `ansi-bright-blue`, etc.
- RGB colors: `#RRGGBB` or `#RGB`
- Attributes: `bold`, `italic`, `underline`, `reverse`
- Background: `on <color>`

## Configuration Examples

### Complete Editing Configuration

Here's a comprehensive example for `~/.cjshrc`:

```bash
# Multiline settings
cjshopt multiline on
cjshopt multiline-indent on
cjshopt line-numbers relative
cjshopt current-line-number-highlight on

# Completion settings
cjshopt completion-preview on
cjshopt auto-tab off
cjshopt completion-case off  # Case-insensitive completions
cjshopt completion-spell on

# Hint settings
cjshopt hint on
cjshopt hint-delay 100  # 100ms delay before showing hints

# Help settings
cjshopt inline-help on

# History
cjshopt set-history-max 10000

# Key bindings
cjshopt keybind profile set emacs

# Syntax highlighting styles
cjshopt style_def ic-keyword "bold blue"
cjshopt style_def ic-command "green"
cjshopt style_def ic-error "bold red"
cjshopt style_def ic-string "#ffaa00"
cjshopt style_def ic-comment "italic #888888"
cjshopt style_def ic-operator "bold"
```

### Minimal Configuration

For a minimal, fast setup:

```bash
# Use the --minimal flag at startup, or configure selectively:
cjshopt multiline on
cjshopt line-numbers off
cjshopt completion-preview off
cjshopt hint off
cjshopt inline-help off
```

## Performance Considerations

### Optimization Tips

1. **Disable unnecessary features** in resource-constrained environments
2. **Increase hint delay** if experiencing lag while typing
3. **Reduce history size** for faster search
4. **Use simpler themes** to reduce prompt rendering time

### Benchmarks

Isocline is designed to be fast and responsive:
- Completion generation: < 10ms for typical cases
- Syntax highlighting: Real-time with no noticeable lag
- History search: Fast even with large history files
- Multiline editing: Smooth for inputs up to hundreds of lines

## Troubleshooting

### Common Issues

**Completions not working:**
- Check if completions are enabled (not started with `--no-completions`)
- Verify PATH contains command directories
- Check file permissions

**Syntax highlighting not showing:**
- Ensure not started with `--no-syntax-highlighting`
- Check terminal color support
- Verify theme styles are defined

**Key bindings not working:**
- Check terminal emulator key sending
- Verify with `cjshopt keybind list`
- Some terminals may not support all key combinations

**Multiline issues:**
- Verify multiline is enabled: `cjshopt multiline status`
- Check for terminal compatibility
- Ensure terminal width is adequate

### Getting Help

- Press `F1` during input for interactive help
- Run `cjshopt <subcommand> --help` for command-specific help
- Check logs in debug mode
- Report issues on the CJsShell GitHub repository