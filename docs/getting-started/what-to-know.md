# What You Need to Know

Welcome to CJ's Shell (cjsh)! This guide highlights the interactive features that ship enabled by default and explains how to tailor them to your workflow.

## Visual & Interface Features

### Prompt Styling & Themes
**Status:** Enabled  
**Configure:** Edit `PS1`, `RPS1`, `PS2`, and `PROMPT_COMMAND` directly in your config using the BBCode-style markup described in [Prompt Markup and Styling](../themes/thedetails.md). Use `cjshopt style_def <token_type> <style>` to redefine highlight palettes that are shared between syntax highlighting and prompt tags.  
**Disable:** Start cjsh with `--minimal` (turns off prompt themes/colors, completions, syntax highlighting, smart `cd`, rc sourcing, the title line, history expansion, multiline line numbers, and auto-indentation), or disable smart cd directly with `cjsh --no-smart-cd` / `cjshopt smart-cd off`.

All prompt styling now lives inside your dotfiles—no external theme DSL or bundled theme directory is required. Share a prompt by exporting new variables or sourcing a file that sets them, just like any other shell configuration.

### True Color Support
**Status:** Enabled when the terminal advertises 24-bit color.  
**Configure:** Automatically detected; adjust styling through your prompt definitions or `cjshopt style_def`.  
**Disable:** Launch with `cjsh --no-colors` or persist by adding `cjshopt login-startup-arg --no-colors` to `~/.cjprofile`.

### Prompt Cleanup & Layout
**Status:** `prompt-cleanup`, `prompt-cleanup-newline`, `prompt-cleanup-empty-line`, and `prompt-cleanup-truncate` default to off so prompts stay exactly as rendered; `prompt-newline` is also off by default.  
**Configure:**

```bash
cjshopt prompt-cleanup on|off|status
cjshopt prompt-cleanup-newline on|off|status
cjshopt prompt-cleanup-empty-line on|off|status
cjshopt prompt-cleanup-truncate on|off|status
cjshopt prompt-newline on|off|status
```

These toggles control whether the previous prompt is erased, whether spacer lines are inserted before or after cleanup, and whether multiline prompts collapse to a single line once a command runs. Enable them to keep transcripts compact; leave them off for a literal transcription of your prompts.

### Syntax Highlighting
**Status:** Enabled  
**Configure:** Change token styles with `cjshopt style_def <token_type> <style>`.  
**Disable:** Start cjsh with `--no-syntax-highlighting` or add `cjshopt login-startup-arg --no-syntax-highlighting` inside `~/.cjprofile`.

---

## Completion & Line Editing

### Fuzzy Completions
**Status:** Enabled (always on)  
Fuzzy matching powers command, path, and argument completions without additional configuration.

### Completion Preview
**Status:** Enabled  
**Configure:** `cjshopt completion-preview on|off|status`

### Spell Correction
**Status:** Enabled  
**Configure:** `cjshopt completion-spell on|off|status`

### Completion Case Sensitivity
**Status:** Disabled (completions are case-insensitive)  
**Configure:** `cjshopt completion-case on|off|status`

### Auto-Tab Expansion
**Status:** Disabled  
**Configure:** `cjshopt auto-tab on|off|status` (auto-inserts a completion when the match is unique)

### Smart cd
**Status:** Enabled  
**Configure:** `cjshopt smart-cd on|off|status`  
**Disable:** Launch with `cjsh --no-smart-cd` or add `cjshopt login-startup-arg --no-smart-cd` to `~/.cjprofile`.

### Inline Hints & Delay
**Status:** Enabled  
**Configure:**

```bash
cjshopt hint on|off|status
cjshopt hint-delay <milliseconds|status>
```

### Inline Help Prompts
**Status:** Enabled  
**Configure:** `cjshopt inline-help on|off|status`

### Status Hint Banner
**Status:** `normal` (only shows when both the buffer and status line are empty)  
**Configure:** `cjshopt status-hints <off|normal|transient|persistent|status>`

### Status Line Visibility
**Status:** Enabled  
**Configure:** `cjshopt status-line on|off|status` (turn it off to hide both syntax feedback and the hint banner entirely)

### Status Reporting
**Status:** Enabled  
**Configure:** `cjshopt status-reporting on|off|status` (leave the status line visible but mute cjsh’s validation/error summaries)

### Visible Whitespace Markers
**Status:** Disabled  
**Configure:** `cjshopt visible-whitespace on|off|status`

### Multiline Editing
**Status:** Enabled  
**Configure:**

```bash
cjshopt multiline on|off|status
cjshopt multiline-indent on|off|status
cjshopt multiline-start-lines <count|status>
cjshopt line-numbers <absolute|relative|off|status>
cjshopt current-line-number-highlight on|off|status
```

---

## Navigation Features

### Smart `cd`
**Status:** Enabled  
Smart `cd` adds fuzzy directory matching for quick navigation between directories.

### Directory Listings
cjsh leaves directory listing behavior up to your configuration. Add an `ls` wrapper, hook, or alias in `~/.cjshrc` if you prefer automatic listings after `cd`.

---

## History Features

- **History expansions:** Enabled in interactive sessions (`!!`, `!$`, etc.). Disable with `cjsh --no-history-expansion` or persist by adding `cjshopt login-startup-arg --no-history-expansion` to `~/.cjprofile`.
- **Reverse search:** Press `Ctrl+R` for the fuzzy history search menu (use `Alt+C` inside it to toggle case sensitivity).
- **History search case sensitivity:** Matching is case-sensitive by default; adjust with `cjshopt history-search-case on|off|status` to set the default for every session.
- **Persistence:** History entries are appended to `~/.cache/cjsh/history.txt`; duplicate commands are suppressed by default.
- **Retention:** Adjust limits with `cjshopt set-history-max <number|default|status>` (any non-negative value; default 1000 entries).

---

## Configuration & Compatibility

### Startup Files
- `~/.cjprofile` – Executed for login shells before interactive setup.
- `~/.cjshrc` – Interactive configuration (aliases, prompt definitions, hooks, etc.).
- `~/.cjsh_logout` – Optional cleanup script sourced on exit.

### Persisting Startup Flags
`cjshopt login-startup-arg` is only valid while configuration files are being sourced. Call it once per flag inside `~/.cjprofile`:

```bash
# ~/.cjprofile
cjshopt login-startup-arg --no-colors
cjshopt login-startup-arg --show-startup-time
```

Supported flags: `--login`, `--interactive`, `--posix`, `--no-colors`, `--no-titleline`, `--show-startup-time`, `--no-source`, `--no-completions`, `--no-completion-learning`, `--no-smart-cd`, `--no-script-extension-interpreter`, `--no-syntax-highlighting`, `--no-history-expansion`, `--no-sh-warning`, `--minimal`, `--secure`, and `--startup-test`.

### POSIX & Bash Compatibility
cjsh targets high POSIX coverage for scripting while providing POSIX+ extensions such as `[[ ... ]]`, brace expansion, here-strings, process substitution, and rich redirection semantics. Syntax extensions are available in scripts and interactive sessions; interactive-only features like history expansion, completions, and prompt styling disable themselves automatically when stdin is not a tty. Use `--minimal` or `--secure` when you want fewer extras in interactive shells.

### Completion Learning Paths
When cjsh scrapes man pages for completions, it uses `man` from `PATH` by default. Set
`CJSH_MAN_PATH` to force a specific `man` binary. In secure mode (`--secure`), cjsh only uses
`CJSH_MAN_PATH` and skips scraping if it is not set or invalid.

When `cjsh` is symlinked or launched as `sh`, interactive sessions print a reminder that cjsh is not a drop-in 100% POSIX shell. Suppress this notice with `cjsh --no-sh-warning` or persist the choice by adding `cjshopt login-startup-arg --no-sh-warning` inside `~/.cjprofile`.

---

## Extended Interactive Tools

### Abbreviations
Define inline expansions with the `abbr` builtin:

```bash
abbr gs='git status --short --branch'
abbr                 # list abbreviations
unabbr gs            # remove an abbreviation
```

### Typeahead
Keystrokes are buffered while commands run so no input is lost. This is always enabled.

### Key Bindings
Inspect or tweak key bindings with:

```bash
cjshopt keybind list                   # safe at runtime
cjshopt keybind profile list           # show available profiles
cjshopt keybind profile set vi         # choose vi bindings (persist in ~/.cjshrc)
cjshopt keybind set <action> <keys>    # redefine bindings (run from config files)
cjshopt keybind add <action> <keys>    # append bindings (run from config files)
```

Use `cjshopt keybind --help` for the full action catalog. For custom widgets, see the `cjsh-widget` builtin in the reference documentation.

---

## Getting Help

- `help` – Overview of built-in commands.
- `help <builtin>` – Detailed usage for a specific builtin.
- `cjshopt --help` and `cjshopt <subcommand> --help` – Configuration guidance.
- Documentation lives under `docs/reference/` for deeper dives into editing, scripting, hooks, and prompt styling.

---

## Quick Configuration Examples

```bash
# Toggle completion preview for the current session
cjshopt completion-preview off

# Enable inline whitespace markers
cjshopt visible-whitespace on

# Switch to vi key bindings (add to ~/.cjshrc to persist)
cjshopt keybind profile set vi

# Increase history retention
cjshopt set-history-max 20000
```

Persist startup flags by placing commands like the following in `~/.cjprofile`:

```bash
cjshopt login-startup-arg --no-colors
```

Run `cjshopt --help` for a complete list of interactive toggles and their detailed help screens.
