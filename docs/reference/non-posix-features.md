# POSIX+ Interactive Features

CJ's Shell aims to behave like `sh` when running scripts while offering an opinionated interactive
experience that goes well beyond the POSIX specification. This document calls out the major
non-POSIX features so you know what is different, how to tune it, and how to disable it when
necessary.

## Prompt System & Visual Enhancements

- **Markup-driven prompts** – `PS1`, `RPS1`, and `PS2` accept BBCode-style markup. Tags such as
  `[b]`, `[color=#ff69b4]`, `[ic-hint]`, and `[bgcolor=ansi-darkgray]` are specific to CJSH's
  integration with the isocline editor.
- **Prompt cleanup controls** – `cjshopt prompt-cleanup*` toggles remove the previous prompt, insert
  spacer lines, or truncate multiline prompts. These behaviors are not part of traditional POSIX
  shells.
- **Right-prompt cursor tracking** – `cjshopt right-prompt-follow-cursor` lets the inline right
  prompt move with the active cursor row, something stock POSIX shells do not support.
- **Dynamic title line** – The introductory banner and title-line management (enabled by default)
  can be disabled with `--no-titleline` or `cjshopt login-startup-arg --no-titleline`.

See [Prompt Markup and Styling](../themes/thedetails.md) for the full markup reference.

## Interactive Editing Extensions

- **Multiline editor** – Automatic continuation detection, optional relative or absolute line
  numbers, inline hints, and visible whitespace markers are provided by isocline and configured via
  `cjshopt` commands (`multiline`, `line-numbers`, `hint`, `visible-whitespace`, etc.).
- **Syntax highlighter** – Real-time token classification and styling via `cjshopt style_def`.
  Highlight categories such as `unknown-command`, `ic-linenumber-current`, and `ic-hint` are
  non-standard.
- **Key binding profiles** – Vi/Emacs modes plus user-defined command bindings using
  `cjshopt keybind` and `cjshopt keybind ext`.
- **Fish-style abbreviations** – `abbr` and `unabbr` provide inline text expansions, a feature not
  present in POSIX shells.

## Completion Engine

- **Fuzzy matching and spell correction** – Configurable through `cjshopt completion-case`,
  `cjshopt completion-spell`, and `cjshopt completion-preview`.
- **Man-page scraping** – `generate-completions` and on-demand parsing of `man` pages populate a
  cache under `~/.cache/cjsh/generated_completions/` for rich option and subcommand help.
- **Inline preview pane & source annotations** – Completion menus display descriptions, origins
  (history, PATH, builtin), and exit-status tags; these are CJSH-specific niceties.

Consult the [Completion Authoring Guide](completions.md) for cache format and customization tips.

## Command Enhancements

- **Smart `cd`** – When `config::smart_cd_enabled` is true (the default), `cd` falls back to fuzzy
  directory matching and prioritizes frequently visited locations. Disable with `--no-smart-cd` or
  `cjshopt login-startup-arg --no-smart-cd` to regain POSIX behavior.
- **`cjshopt`** – A dedicated configuration builtin that sets editor behaviour, generates config
  files, and manages key bindings/history limits. None of its subcommands exist in POSIX sh.
- **`cjsh-widget`** – Exposes the line editor to shell scripts for advanced key-driven workflows.
- **`generate-completions`** – Pre-warm the completion cache. A convenience command beyond POSIX.
- **`hook`** – Lightweight precmd/preexec/chpwd hook management similar to zsh's hook system.

## History & Execution Utilities

- **History expansion** – Bash-style tokens (`!!`, `!$`, `!^`, `^foo^bar`, etc.) work in interactive
  mode. They automatically disable in script mode, `cjsh -c`, or when stdin is not a tty. Use
  `--no-history-expansion` or `cjshopt login-startup-arg --no-history-expansion` to turn it off.
- **Persistent exit codes** – Each history entry records the command's exit status to enrich
  completions and prompts.
- **Typeahead buffering** – Key presses made while a command runs are replayed automatically once
  the prompt returns.

## Additional Non-POSIX Behaviours

- **Startup diagnostics** – `--show-startup-time` prints the duration spent initializing CJSH.
- **Secure mode** – `--secure` skips all profile/rc/logout sourcing for hardened sessions.
- **Consistent error output** – Interpreter failures now use the same compact `cjsh:` error_out
  format as other builtins for predictable logs.

## Disabling Enhancements

CJSH keeps POSIX-focused scripting predictable by disabling most POSIX+ features automatically in
non-interactive contexts. When you need a strictly standard shell interactively, start with
`--minimal` (disables colors, completions, syntax highlighting, smart `cd`, rc sourcing, the title
line, history expansion, multiline line numbers, and auto-indentation) and add `--secure` if you
also want to skip all login/logout dotfiles:

```bash
cjsh --minimal --secure
```

Or persist equivalent flags through `cjshopt login-startup-arg` in `~/.cjprofile`.

## Summary

- Prompt markup, cleanup toggles, and syntax styling provide rich visual customization beyond POSIX.
- The isocline editor delivers multiline editing, hints, completions, abbreviations, and keymap
  control.
- Builtins such as `cjshopt`, `generate-completions`, `hook`, and `cjsh-widget` extend the shell's
  capabilities.
- History expansion, typeahead buffering, and persistent exit codes streamline interactive work.

Each enhancement is optional and either automatically disabled outside of interactive mode or
controllable through command-line flags and `cjshopt` commands, allowing you to match the behaviour
you need for any environment.
