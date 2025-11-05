# Prompt Markup and Styling

CJ's Shell no longer relies on an external theme DSL. Prompt styling is handled directly through
the standard shell prompt variables (`PS1`, `RPS1`/`RPROMPT`, and `PROMPT_COMMAND`) combined with
inline BBCode-style markup. This page explains how the markup works, which escape sequences are
available, and how to persist your preferred prompt layout.

## Quick Start

- Set `PS1` to control the primary prompt. The default template is:
  ```bash
  PS1='[!red][[/red][yellow]\u[/yellow][green]@[/green][blue]\h[/blue] [color=#ff69b4]\w[/color][!red]][/red][!b] \$ [/b]'
  ```
- Set `RPS1` (or `RPROMPT`) to control the right-aligned prompt. The default is `[ic-hint]\A[/ic-hint]`.
- Use `PROMPT_COMMAND` for commands that should run before each prompt.
- Apply markup directly inside these variables to style text, add colors, and align sections.

```bash
export PS1='[b hotpink]\u[/b] in [color=#87ceeb]\w[/color]\n$ '
export RPS1='[dim]\A[/dim]'
export PROMPT_COMMAND='__update_git_info'
```

> Markup is parsed by the isocline line editor. Invalid tags are ignored; run with
> `ISOCLINE_BBCODE_DEBUG=1` to print debugging information if you are experimenting.

## Markup Building Blocks

### Inline Tags

- `[b]…[/b]` – bold text
- `[i]…[/i]` – italics
- `[u]…[/u]` – underline
- `[r]…[/r]` – reverse video
- `[dim]…[/dim]`, `[strike]…[/strike]`, `[blink]…[/blink]`, `[hidden]…[/hidden]`
- `[color=<name|#RRGGBB>]…[/color]` – foreground color
- `[bgcolor=<name|#RRGGBB>]…[/bgcolor]` – background color
- `[ansi-sgr=sequence]…[/]` – inject raw SGR codes (advanced)
- `[width=columns;<align>;<fill>;dots]…[/]` – restrict content width (align is `left|center|right`)

Color names include the full HTML color table plus `ansi-` variants such as `ansi-red`,
`ansi-lightgray`, and `ansi-teal`. Hex values (`#ff69b4`) are also supported.

Tag attributes can be combined inline: `[b color=hotpink]…[/]` applies both bold and the color.

### Named Styles

The syntax highlighter exposes reusable style names that can be used as tags. The defaults come
from `token_constants::default_styles` and include:

```
unknown-command  colon  path-exists  path-not-exists
glob-pattern     operator  keyword  builtin  system
variable         assignment-value  string  comment
command-substitution  arithmetic  option  number
function-definition  history-expansion
ic-prompt  ic-hint  ic-error  ic-info  ic-emphasis
ic-source  ic-diminish  ic-bracematch  ic-whitespace-char
ic-linenumbers  ic-linenumber-current
```

Use them as simple tags: `[ic-hint]⌛[/ic-hint]`. Redefine a style with
`cjshopt style_def <name> <style>` to change both the syntax highlighter and any markup that uses
that tag.

### Closing Tags

Use `[/tag]` to end a specific style or `[/]` to end the most recent open tag. You can also nest
tags freely; the parser keeps a stack so `[b][color=hotpink]...[/color][/b]` works as expected.

## Prompt Escape Sequences

Prompt templates use familiar POSIX/Bash escapes. CJ's Shell expands the following sequences:

- `\a` bell character
- `\d` locale-specific date (e.g., `Tue Mar 04`)
- `\D{fmt}` date formatted with `strftime`
- `\e` / `\E` escape character (`\033`)
- `\h` short hostname, `\H` full hostname
- `\j` number of background jobs
- `\l` current tty
- `\n` newline, `\r` carriage return
- `\s` shell name (`cjsh` by default)
- `\t` current time `HH:MM:SS`, `\T` 12-hour, `\@` 12-hour + AM/PM, `\A` `HH:MM`
- `\u` username
- `\v` short cjsh version, `\V` full version string
- `\w` working directory with `$HOME` shortened to `~`, `\W` basename of working directory
- `\$` `#` for root, otherwise `$`
- `\?` exit status of last command
- `\\` literal backslash, `\[` and `\]` for zero-width control regions
- Octal escapes (`\033`) for arbitrary characters

All escape processing happens before markup is interpreted.

## Right-Aligned Prompt and Continuation Lines

- `RPS1` (preferred) or `RPROMPT` controls the inline right prompt. Markup and escapes work the
  same way as `PS1`.
- `PS2` is used automatically for continuation lines by the line editor. Set it if you want a
  custom secondary prompt, e.g. `export PS2='[dim]> [/dim]'`.
- `PROMPT_COMMAND`, when set, runs before CJ's Shell generates `PS1`/`RPS1`. Use it to refresh
  environment variables, collect Git metadata, or update the terminal title.

## Prompt Cleanup and Layout Options

Use `cjshopt` to control how the prompt behaves after command execution:

```bash
cjshopt prompt-cleanup on|off|status          # Remove the previous prompt when you press Enter
cjshopt prompt-cleanup-newline on|off|status  # Insert a blank line before the next prompt
cjshopt prompt-cleanup-empty-line on|off      # Emit an extra spacer line during cleanup
cjshopt prompt-cleanup-truncate on|off        # Collapse multiline prompts to a single line
cjshopt prompt-newline on|off                 # Always print a blank line after commands
```

Each toggle prints guidance on how to persist the setting. Add the chosen command to `~/.cjshrc`
to make it stick.

## Examples

```bash
# Minimal prompt that still highlights errors
export PS1='[ic-error]\$[/ic-error] '

# Git-aware prompt via PROMPT_COMMAND
prompt_git_info() {
  if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    local branch
    branch=$(git branch --show-current 2>/dev/null)
    export GIT_SEG="[color=gold](${branch:-detached})[/color] "
  else
    unset GIT_SEG
  fi
}
export PROMPT_COMMAND=prompt_git_info
export PS1='[b]\u[/b] [color=#6cb6ff]\w[/color] ${GIT_SEG}\$ '

# Right prompt that shows the time and exit code when non-zero
export RPS1='[dim]\A[/dim][if $?>0][space][ic-error]✗ $?[ic-error][/if]'
```

The last example demonstrates mixing markup with shell substitutions. Because prompt evaluation
occurs inside CJ's Shell, you can use regular shell parameter expansion to build dynamic strings
before markup is parsed.

## Persisting Configuration

- Add `export` statements to `~/.cjshrc` for prompt variables.
- Use `cjshopt style_def` in the same file to adjust highlight palettes.
- When creating configuration files through `cjshopt generate-rc` or `cjshopt generate-profile`,
  you can drop your prompt definitions into the generated files directly.

That is all you need to theme CJ's Shell now—no external DSL, no extra tooling. Compose the prompt
you want with markup, save it in your rc files, and CJSH will handle the rest.