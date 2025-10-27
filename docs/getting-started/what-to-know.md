# What You Need to Know

Welcome to CJ's Shell (cjsh)! This guide highlights the interactive features that ship enabled by default and explains how to tailor them to your workflow.

## Visual & Interface Features

### Themes
**Status:** Enabled  
**Configure:** Load a theme with `source path/to/theme.cjsh` or embed a `theme_definition` directly inside `~/.cjshrc`.  
**Disable:** Start cjsh with `--no-themes` or add `cjshopt login-startup-arg --no-themes` to `~/.cjprofile`.

Themes live in the `themes/` directory and can be customized or authored from scratch using the theme DSL. Sourcing a theme in `~/.cjshrc` applies it automatically on startup.

### True Color Support
**Status:** Enabled when the terminal advertises 24-bit color.  
**Configure:** Automatically detected; adjust styling through theme definitions.  
**Disable:** Launch with `cjsh --no-colors` or persist by adding `cjshopt login-startup-arg --no-colors` to `~/.cjprofile`.

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
**Disable:** Run cjsh with `--no-smart-cd` or add `cjshopt login-startup-arg --no-smart-cd` to `~/.cjprofile`.

Smart `cd` adds fuzzy directory matching and automatically records bookmarks. Manage bookmarks with:

```bash
cjshopt set-max-bookmarks <number>    # limit stored locations (10–1000, default 100)
cjshopt bookmark-blacklist list       # inspect ignored paths
cjshopt bookmark-blacklist add <path>
cjshopt bookmark-blacklist remove <path>
```

### Directory Listings
cjsh leaves directory listing behavior up to your configuration. Add an `ls` wrapper, hook, or alias in `~/.cjshrc` if you prefer automatic listings after `cd`.

---

## History Features

- **History expansions:** Enabled in interactive sessions (`!!`, `!$`, etc.). Disable with `cjsh --no-history-expansion` or persist by adding `cjshopt login-startup-arg --no-history-expansion` to `~/.cjprofile`.
- **Reverse search:** Press `Ctrl+R` for fuzzy, incremental history search.
- **Persistence:** History entries are appended to `~/.cache/cjsh/history.txt`; duplicate commands are suppressed by default.
- **Retention:** Adjust limits with `cjshopt set-history-max <number|default|status>`.

---

## Configuration & Compatibility

### Startup Files
- `~/.cjprofile` – Executed for login shells before interactive setup.
- `~/.cjshrc` – Interactive configuration (aliases, themes, hooks, etc.).
- `~/.cjsh_logout` – Optional cleanup script sourced on exit.

### Persisting Startup Flags
`cjshopt login-startup-arg` is only valid while configuration files are being sourced. Call it once per flag inside `~/.cjprofile`:

```bash
# ~/.cjprofile
cjshopt login-startup-arg --no-themes
cjshopt login-startup-arg --no-smart-cd
cjshopt login-startup-arg --show-startup-time
```

Supported flags: `--login`, `--interactive`, `--debug`, `--no-themes`, `--no-colors`, `--no-titleline`, `--show-startup-time`, `--no-source`, `--no-completions`, `--no-syntax-highlighting`, `--no-smart-cd`, `--no-prompt`, `--minimal`, and `--startup-test`.

### POSIX & Bash Compatibility
cjsh targets high POSIX coverage for scripting while providing POSIX+ extensions such as `[[ ... ]]`, brace expansion, here-strings, process substitution, and rich redirection semantics. POSIX+ behavior is opt-in through flags or configuration.

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

Use `cjshopt keybind --help` for the full action catalog. For custom widgets, see the `widget` builtin in the reference documentation.

---

## Getting Help

- `help` – Overview of built-in commands.
- `help <builtin>` – Detailed usage for a specific builtin.
- `cjshopt --help` and `cjshopt <subcommand> --help` – Configuration guidance.
- Documentation lives under `docs/reference/` for deeper dives into editing, scripting, hooks, and themes.

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
cjshopt set-history-max 5000
```

Persist startup flags by placing commands like the following in `~/.cjprofile`:

```bash
cjshopt login-startup-arg --no-colors
cjshopt login-startup-arg --no-smart-cd
```

Run `cjshopt --help` for a complete list of interactive toggles and their detailed help screens.
