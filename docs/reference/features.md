# Features Overview

CJ's Shell (cjsh) pairs a standards-focused POSIX shell engine with a modern interactive
experience. The goal is zero-compromise scripting compatibility backed by rich editing tools that
require no third-party plugins.

## Core Shell Engine

- **POSIX-first semantics** – Roughly 95% POSIX coverage with >1500 standards-focused tests.
- **Bourne-compatible surface** – Classic constructs (`if`, `case`, `for`, `while`, functions,
    redirections, here-documents, command substitution) behave the way portable scripts expect.
- **Selective bash extensions** – `[[ … ]]`, arithmetic contexts, here-strings, and history
    expansion ship enabled for interactive use but stay out of the way for scripts.
- **Job control** – Background jobs, `fg`, `bg`, `jobs`, `wait`, `disown`, and `trap` integrate with
    the internal process manager so interactive sessions stay responsive. `set -o huponexit`
    controls whether exiting shells hang up or leave running jobs alone (default: off, so long-lived
    helpers keep running until you explicitly stop them).

## Interactive Layer

Powered by the embedded [isocline](https://github.com/cadenfinley/isocline) editor:

- **Multiline editing** with automatic indentation and optional line numbers.
- **Syntax highlighting** that understands commands, keywords, paths, arguments, substitutions,
    comments, and error states.
- **Fuzzy completions** for commands, files, options, variables, users, and hosts. Completions learn
    from your `PATH` and cached man-page metadata.
- **Inline hints & preview** with configurable delays, spell correction, and case sensitivity.
- **Advanced history** – reverse search (`Ctrl+R`), deduplicated persistent history with exit codes,
    and bash-style history expansion that auto-disables in non-interactive contexts.
- **Custom key bindings** – Emacs and Vi profiles plus fine-grained overrides via
    `cjshopt keybind` (including command-driven bindings through `cjsh-widget`).
- **Typeahead capture** – Keystrokes entered while a command runs are buffered and replayed when the prompt returns so you never lose input.
- **Abbreviations** – `abbr`/`unabbr` provide fish-style expansions for frequently typed snippets.

See the [Interactive Editing Guide](editing.md) and [Completion Authoring Guide](completions.md)
for full details.

## Prompt & Visual Styling

- **BBCode-inspired markup** inside `PS1`, `RPS1`/`RPROMPT`, `PS2`, and other prompt variables.
    Tags such as `[b]`, `[color=hotpink]`, `[ic-hint]`, and `[bgcolor=#202020]` let you mix ANSI
    styles with reusable highlight names. The full markup reference lives in
    [Prompt Markup and Styling](../themes/thedetails.md).
- **Prompt cleanup toggles** via `cjshopt prompt-cleanup*` set whether the previous prompt is
    removed, whether blank lines are inserted, and whether multiline prompts are truncated.
- **Right prompt cursor tracking** – `cjshopt right-prompt-follow-cursor` keeps the inline right
    prompt aligned with the current cursor row instead of pinning it to the first line.
- **`cjshopt style_def`** redefines syntax-highlighter styles (`unknown-command`, `ic-hint`, etc.),
    instantly applying to both inline highlighting and prompt markup tags that reference them.

## Configuration Surface

- **Runtime toggles** – Every major interactive feature has a `cjshopt` command. Highlights:
    - `cjshopt multiline`, `cjshopt multiline-indent`, `cjshopt line-numbers`,
        `cjshopt multiline-start-lines`
     - `cjshopt completion-preview`, `cjshopt completion-case`, `cjshopt completion-spell`,
         `cjshopt completion-learning`, `cjshopt auto-tab`
     - `cjshopt hint`, `cjshopt hint-delay`, `cjshopt inline-help`, `cjshopt status-hints`,
        `cjshopt status-line`, `cjshopt status-reporting`, `cjshopt visible-whitespace`
    - `cjshopt prompt-newline`, `cjshopt prompt-cleanup`, `cjshopt prompt-cleanup-newline`,
        `cjshopt prompt-cleanup-empty-line`, `cjshopt prompt-cleanup-truncate`
    - `cjshopt keybind …` and `cjshopt keybind ext …` for keymap management
    - `cjshopt set-history-max` to adjust persistent history size (0 or more entries; no upper limit)
    - `set -o huponexit` mirrors bash's option for sending SIGHUP/SIGTERM to background jobs when the
        shell exits (off by default so long-running helpers stick around)
- **Login/startup flags** – Place `cjshopt login-startup-arg <flag>` lines in `~/.cjprofile` to
    replay command-line switches (`--minimal`, `--no-completions`, `--show-startup-time`, etc.) on
    every launch. Flags that are valid on the CLI are respected during profile evaluation.
- **Generated config skeletons** – `cjshopt generate-profile`, `cjshopt generate-rc`, and
    `cjshopt generate-logout` create `~/.cjprofile`, `~/.cjshrc`, and `~/.cjsh_logout` (or alternate
    locations under `~/.config/cjsh/`) with sensible defaults.

### Startup Files

| File | When it runs | Typical responsibilities |
| --- | --- | --- |
| `~/.cjprofile` (or `~/.config/cjsh/.cjprofile`) | Login shells before interactive setup | Export environment vars, add `cjshopt login-startup-arg` flags |
| `~/.cjshrc` | Every interactive shell (unless `--no-source`) | Prompt definitions, aliases, key bindings, abbreviations |
| `~/.cjsh_logout` | When a login shell exits | Cleanup hooks, session summaries |

Persistent caches (history, generated completions, etc.) live under `~/.cache/cjsh/`.

## Command-line Flags

`cjsh` accepts these switches (short/long forms shown where available):

- `-h, --help` – usage information
- `-v, --version` – print the version banner and exit
- `-l, --login` – treat the shell as a login shell (source `~/.cjprofile`)
- `-i, --interactive` – force interactive behavior even if stdin is not a tty
- `-c, --command=<string>` – execute a single command and exit (disables history expansion)
- `-m, --minimal` – disable prompt themes/colors, completions, syntax highlighting, rc sourcing,
    the title line, history expansion, multiline line numbers, auto-indentation, and the startup
    time banner
- `-C, --no-colors`
- `-L, --no-titleline`
- `-U, --show-startup-time`
- `-N, --no-source`
- `-O, --no-completions`
- `--no-completion-learning` – keep completions enabled but skip on-demand man-page scraping
- `-S, --no-syntax-highlighting`
- `-H, --no-history-expansion`
- `-W, --no-sh-warning` – suppress the reminder shown when cjsh is invoked via `sh`
- `-s, --secure` – skip `~/.cjprofile`, `~/.cjshrc`, and `~/.cjsh_logout` entirely
- `-X, --startup-test` – diagnostic mode used by the bundled tests

Flags affecting feature toggles take effect early in startup and can also be injected via
`cjshopt login-startup-arg` in configuration files.

## Built-in Tooling Highlights

- `generate-completions` – Pre-populate completion caches by scraping manual pages in parallel.
- `hash` – Inspect or reset execution caches.
- `history` / `fc` – Explore, edit, and replay persistent history (exit codes are stored alongside entries).
- `hook` – Lightweight precmd/preexec/chpwd hook system.
- `cjsh-widget` – Bridge between shell code and the line editor for custom key-driven behaviors.

## Performance Characteristics

- Single statically linked executable (vendored dependencies only).
- Aggressive optimization flags and caching layers (completion caches, prompt helpers, execution
    lookup cache).
- Prompt markup renders quickly because formatting is handled inside the line editor with minimal
    allocations.

## Platform & Build Support

- **Targets** – Linux, macOS, and WSL are primary; other POSIX-like systems generally work.
- **Toolchain** – Requires CMake ≥3.25 and a C++17-capable compiler (clang, GCC, or MSVC via WSL).
- **Quick build** – `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel`.
- **Package installs** – Homebrew (`brew install cjsh`) and Arch AUR (`cjsh`) are maintained.

For installation walkthroughs, see [Quick Start](../getting-started/quick-start.md). For the
interactive feature matrix, continue to the [Editing](editing.md) and
[POSIX+ Interactive Features](non-posix-features.md) documents.
