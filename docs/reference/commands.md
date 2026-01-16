# Built-in Commands Reference

CJ's Shell provides a comprehensive set of built-in commands that are available without requiring external programs. These commands are optimized for both interactive use and shell scripting.

## Navigation and File System

### cd
Change the current working directory.

```bash
cd [directory]
```

- Use `cd` without arguments to go to home directory
- Use `cd -` to switch to the previous directory
- Smart CD is enabled by default with fuzzy matching

### pwd
Print the current working directory.

```bash
pwd
```

## Text Output

### echo
Print arguments separated by spaces.

```bash
echo [args...]
```

### printf
Format and print data using printf-style specifiers.

```bash
printf format [arguments...]
```

## Shell Control

### exit / quit
Leave the shell with an optional exit status.

```bash
exit [n]
quit [n]
```

### help
Display the CJSH command reference.

```bash
help
```

### version
Show cjsh version information.

```bash
version
```

## Script Execution

### source / .
Execute commands from a file in the current shell context.

```bash
source filename [arguments...]
. filename [arguments...]
```

### eval
Evaluate a string as shell code.

```bash
eval [args...]
```

### exec
Replace the shell process with another program.

```bash
exec command [args...]
```

## Variables and Environment

### export
Set or display environment variables.

```bash
export [name[=value]...]
```

### unset
Remove environment variables.

```bash
unset name [name...]
```

### local
Declare local variables inside functions.

```bash
local name[=value] [name[=value]...]
```

### readonly
Mark variables as read-only.

```bash
readonly name[=value] [name[=value]...]
```

### set
Adjust shell options or positional parameters.

```bash
set [options] [args...]
```

- `set -o huponexit` mirrors bash's hangup behavior toggle; when enabled the shell sends
  SIGHUP/SIGTERM to managed background jobs as it exits. Leave it off (the default) to keep helpers
  like dev servers alive until you explicitly stop them.

### shift
Rotate positional parameters to the left.

```bash
shift [n]
```

## Aliases

### alias
Create or list command aliases.

```bash
alias [name[=value]...]
```

### unalias
Remove command aliases.

```bash
unalias name [name...]
```

## Abbreviations

### abbr
Create, update, or list fish-style abbreviations that expand during interactive editing.

```bash
abbr [name=expansion ...]
```

- Run without arguments to display all configured abbreviations
- Use `name=expansion` pairs to set or update entries
- Triggers that contain whitespace are rejected
- Abbreviations expand when the trigger is followed by whitespace or when the line is submitted
- Two defaults are shipped with cjsh: `abbr` → `abbreviate` and `unabbr` → `unabbreviate`

### unabbr
Remove one or more fish-style abbreviations.

```bash
unabbr name [name...]
```

- Removing a non-existent abbreviation prints a warning but does not stop processing the rest
- Pair with `abbr` to keep a clean set of triggers in your session configuration

## Control Flow

### if
Run conditional blocks in scripts.

```bash
if condition; then
    commands
[elif condition; then
    commands]
[else
    commands]
fi
```

### test / [
Evaluate POSIX test expressions.

```bash
test expression
[ expression ]
```

### [[
Evaluate extended test expressions (bash-style).

```bash
[[ expression ]]
```

### break
Exit the current loop.

```bash
break [n]
```

### continue
Skip to the next loop iteration.

```bash
continue [n]
```

### return
Exit the current function with an optional status.

```bash
return [n]
```

### :
No-op command that always succeeds.

```bash
:
```

### true
Always succeed and return exit status 0. Handy for conditionals or resetting `$?`.

```bash
true
```

### false
Always fail and return exit status 1. Useful for testing error paths or short-circuiting scripts.

```bash
false
```

## Job Control

### jobs
List background jobs.

```bash
jobs
```

### fg
Bring a job to the foreground.

```bash
fg [job_spec]
```

### bg
Resume a job in the background.

```bash
bg [job_spec]
```

### wait
Wait for jobs or processes to finish.

```bash
wait [pid|job_spec...]
```

### kill
Send signals to jobs or processes.

```bash
kill [-signal] pid|job_spec
```

### disown
Detach jobs from the shell so they are no longer listed or sent hangup signals when cjsh exits.

```bash
disown [-a|--all] [job_spec...]
```

- With no arguments, the current job is disowned
- `-a/--all` removes every tracked job in one shot
- Disowned jobs continue running even if `set -o huponexit` is enabled later in the session

### jobname
Assign or update a friendly display name for a tracked job. The name shows up in `jobs`, `fg`, `bg`,
and completions while the job remains in the table.

```bash
jobname JOB_SPEC NEW_NAME
```

- `JOB_SPEC` can be a `%job_id`, a PID, or any command prefix that normally resolves jobs (`fg`
and `bg`-style matching)
- `NEW_NAME` is treated as the rest of the command line after `JOB_SPEC`, so spaces are allowed
- Names must contain at least one non-whitespace character; use the original command to revert

## Signal Handling

### trap
Set signal handlers or list existing traps.

```bash
trap [action] [signal...]
```

## Command Information

### type
Explain how a command name will be resolved.

```bash
type name [name...]
```

### which
Locate executables in PATH.

```bash
which name [name...]
```

### hash
Cache command lookups or display the cache.

```bash
hash [-r] [name...]
```

### generate-completions
Regenerate cached completion metadata for external commands.

```bash
generate-completions [--quiet] [--no-force] [-j jobs] [command ...]
```

- Runs against every executable discoverable in `PATH` when no command list is supplied
- `--quiet` suppresses per-command status messages and prints only a summary (failures are listed)
- `--no-force` keeps existing cache files and only generates data for missing entries
- `-j/--jobs` limits the number of commands processed simultaneously (defaults to CPU count)
- Use `--` to end option parsing when processing command names that start with a dash

### builtin
Run a builtin directly, bypassing functions and PATH.

```bash
builtin command [args...]
```

### command
Execute a command while bypassing shell functions or print information about it.

```bash
command [-pVv] COMMAND [ARG...]
```

**Options:**
- `-p` – Temporarily use the default `PATH` of `/usr/bin:/bin` when resolving `COMMAND`
- `-v` – Print a short description of how `COMMAND` would be resolved
- `-V` – Print a verbose description (builtin, full path, or not found)

When no inspection flags are supplied, `command` runs the target using the shell's execution
engine, allowing you to bypass shell functions that shadow external commands. The command returns
the exit status of the invoked program.

## Hook System

### hook
Manage shell hooks that run at key lifecycle moments.

```bash
hook <add|remove|list|clear> [hook_type] [function_name]
```

Hook types include `precmd`, `preexec`, and `chpwd`. Use `hook add` inside configuration files to
register functions and `hook list` to inspect active hooks. See
[`hooks.md`](hooks.md) for complete examples and best practices.

## Input/Output

### read
Read user input into variables.

```bash
read [options] name [name...]
```

### getopts
Parse positional parameters as short options.

```bash
getopts optstring name [args...]
```

## History

### history
Display command history.

```bash
history [n]
```

- Without arguments, displays all history entries
- With a number `n`, displays the last `n` entries
- History is stored in `~/.cache/cjsh/history.txt`

### fc
Fix Command - edit and re-execute commands from history (POSIX-compliant).

```bash
fc [-e editor] [-ln] [first [last]]
```

**Options:**
- `-e editor` - Use specified editor (default: `$FCEDIT`, `$EDITOR`, or `vi`)
- `-l` - List commands instead of editing
- `-n` - Suppress line numbers when listing
- `-r` - Reverse order of commands when listing

**Arguments:**
- `first` - First command to edit/list (default: previous command)
- `last` - Last command to edit/list (default: same as first)

**Examples:**

```bash
# Edit the previous command in your editor
fc

# Edit command number 53
fc 53

# Edit commands 10 through 15
fc 10 15

# List last 16 commands
fc -l

# List commands 10 through 20
fc -l 10 20

# Edit with a specific editor
fc -e nano

# Use with environment variables
export FCEDIT=nano
fc  # Will use nano as the editor
```

**How it works:**
1. Opens the specified command(s) in your editor
2. When you save and exit the editor, the modified command is displayed
3. The modified command is automatically executed
4. The result is added to history

**Environment Variables:**
- `FCEDIT` - Preferred editor for `fc` (checked first)
- `EDITOR` - Fallback editor if `FCEDIT` is not set
- Default is `ed` if neither variable is set (POSIX requirement)

**Tip:** For a better editing experience, set your preferred editor:
```bash
export FCEDIT=vi     # or nano, emacs, etc.
export EDITOR=vi     # fallback for other tools too
```

## System Information

### times
Show CPU usage for the shell and its children.

```bash
times
```

### umask
Show or set the file creation mask.

```bash
umask [mode]
```

### ulimit
Inspect or adjust resource limits for the current shell and any processes it launches.

```bash
ulimit [-HS] [-a] [-f | -n | -t | ...] [limit]
```

- `-a` lists every supported limit along with the active hard/soft values.
- Use `-H` or `-S` to operate on the hard or soft limit respectively (defaults to soft).
- Resource selectors mirror the underlying OS (`-f` file size, `-n` open files, `-t` CPU time, `-v` virtual memory, etc.). Unsupported switches print a descriptive error.
- Limits accept numeric values, or the keywords `unlimited`, `hard`, or `soft`.

Run `ulimit --help` for the full table of supported options on your platform.

## Theming and Customization


### Prompt Styling
cjsh reads the standard prompt variables, so customizing the look is as simple as exporting new values inside your configuration files:

```bash
export PS1='[b]\u[/b] [color=#87ceeb]\w[/color]\n$ '
export RPS1='[ic-hint]\A[/ic-hint]'
export PROMPT_COMMAND='__update_git_info'
```

Add snippets like these to `~/.cjshrc` (or any sourced config) to persist them across sessions. See [Prompt Markup and Styling](../themes/thedetails.md) for the full markup reference plus additional examples.

### cjshopt
Generate config files and adjust cjsh options.

```bash
cjshopt <subcommand> [options]
```

Available subcommands:
- `style_def` - Define or redefine syntax highlighting styles
- `login-startup-arg` - Add startup flags (config file only)
- `completion-case` - Configure completion case sensitivity
- `completion-spell` - Toggle spell correction suggestions in completions
- `line-numbers` - Configure line numbers in multiline input (on/off/relative/absolute)
- `line-numbers-replace-prompt` - Replace the final prompt line with the line-number gutter
- `current-line-number-highlight` - Toggle highlighting of the current line number
- `hint-delay` - Set hint display delay in milliseconds
- `completion-preview` - Configure completion preview
- `visible-whitespace` - Toggle visible whitespace characters in the editor
- `hint` - Configure inline hints
- `multiline-indent` - Configure auto-indent in multiline input
- `multiline` - Configure multiline input mode
- `inline-help` - Configure inline help messages
- `auto-tab` - Configure automatic tab completion
- `prompt-newline` - Force a blank line after each command
- `prompt-cleanup` - Enable/disable removal of the previous prompt after execution
- `prompt-cleanup-newline` - Control whether cleanup inserts a newline before the next prompt
- `prompt-cleanup-empty-line` - Insert an empty spacer line during cleanup
- `prompt-cleanup-truncate` - Collapse multiline prompts when cleanup runs
- `keybind` - Inspect or modify key bindings (modifications config file only)
- `generate-profile` - Create or overwrite ~/.cjprofile (use `--alt` for `~/.config/cjsh/.cjprofile`)
- `generate-rc` - Create or overwrite ~/.cjshrc (use `--alt` for `~/.config/cjsh/.cjshrc`)
- `generate-logout` - Create or overwrite ~/.cjsh_logout (use `--alt` for `~/.config/cjsh/.cjsh_logout`)
- `set-history-max` - Configure history persistence limits
- `set-completion-max` - Limit the maximum number of completion suggestions shown

### cjsh-widget
Interact with the embedded line editor (isocline) to drive advanced key bindings.

```bash
cjsh-widget <subcommand> [...]
```

Available subcommands include:
- `get-buffer` / `set-buffer` – Read or replace the active input buffer
- `get-cursor` / `set-cursor` – Inspect or move the cursor (byte offsets)
- `insert` / `append` / `clear` – Modify buffer contents near the cursor or reset the line
- `accept` – Simulate pressing Enter to submit the current buffer

These commands are primarily used from custom key bindings and widgets rather than typed
interactively. Combine them with `cjshopt keybind ext` inside `~/.cjshrc` to create bespoke
editing behaviors.

#### login-startup-arg

Persist startup flags that should be applied before the interactive configuration is sourced. This command is only honored inside startup files such as `~/.cjprofile`; running it at an interactive prompt prints an error.

```bash
cjshopt login-startup-arg <flag>
```

Supported flags:

| Flag | Effect |
| --- | --- |
| `--login` | Mark the current shell instance as a login shell |
| `--interactive` | Force interactive startup behavior |
| `--no-colors` | Disable colorized prompt and syntax output |
| `--no-titleline` | Disable terminal title updates on startup |
| `--show-startup-time` | Print the time spent initializing cjsh |
| `--no-source` | Skip sourcing interactive configuration files |
| `--no-completions` | Skip completion initialization |
| `--no-syntax-highlighting` | Disable syntax highlighting in the editor |
| `--no-smart-cd` | Disable fuzzy/smart directory changes |
| `--no-history-expansion` | Disable `!!`, `!$`, and related history tokens |
| `--minimal` | Disable cjsh extras (colors, completions, smart cd, sourcing, etc.) |
| `--secure` | Skip profile/rc/logout sourcing even for login shells |
| `--startup-test` | Enable startup test mode |

Add one line per flag in `~/.cjprofile` to persist the desired behavior:

```bash
# Inside ~/.cjprofile
cjshopt login-startup-arg --minimal
cjshopt login-startup-arg --show-startup-time
```

#### completion-case

Toggle whether tab completions treat case as significant. Synonyms such as `enable`, `disable`, `true`, and `false` are also accepted for convenience.

```bash
cjshopt completion-case <on|off|status>
```

Examples:

```bash
cjshopt completion-case on      # Case-sensitive matching
cjshopt completion-case off     # Case-insensitive matching (default)
cjshopt completion-case status  # Show the current mode
```

Add the command to `~/.cjshrc` if you want the preference remembered across sessions.

#### completion-spell

Enable, disable, or inspect spell correction inside the completion engine. When enabled, cjsh will try to fix minor typos before offering suggestions. The subcommand also accepts synonyms such as `enable`, `disable`, `true`, `false`, and `--status`.

```bash
cjshopt completion-spell <on|off|status>
```

Examples:

```bash
cjshopt completion-spell on       # Turn on spell correction
cjshopt completion-spell status   # Display the current state
```

Persist the choice by placing the command in `~/.cjshrc`.

#### line-numbers

Enable, disable, or inspect line numbers in multiline input mode. When enabled, cjsh will display numbers on the left side of multiline input, making it easier to navigate and edit multi-line commands or scripts. You can choose between absolute numbering (the default) or relative numbering, which shows the distance to the active cursor line.

```bash
cjshopt line-numbers <on|off|relative|absolute|status>
```

Examples:

```bash
cjshopt line-numbers on       # Enable line numbers in multiline input
cjshopt line-numbers relative # Switch to relative numbering
cjshopt line-numbers off      # Disable line numbers in multiline input
cjshopt line-numbers status   # Show the current setting
```

Add the command to `~/.cjshrc` to persist the setting across sessions. The subcommand also accepts synonyms such as `enable`, `disable`, `true`, `false`, `absolute`, and `rel`/`relative`.

> **Tip:** Style the line numbers themselves with `cjshopt style_def ic-linenumbers "color=#FFB86C"` (or any other style). See `cjshopt style_def` for the full list of supported style directives.

#### line-numbers-continuation

Control whether multiline line numbers remain visible when a continuation prompt marker (PS2) is configured. By default, configuring a continuation prompt hides the numbers; enabling this option keeps them aligned with the continuation marker instead.

```bash
cjshopt line-numbers-continuation <on|off|status>
```

Examples:

```bash
cjshopt line-numbers-continuation on      # Keep line numbers even with custom continuation prompts
cjshopt line-numbers-continuation off     # Hide line numbers whenever a continuation prompt exists
cjshopt line-numbers-continuation status  # Show the current setting
```

Add the command to `~/.cjshrc` to persist the setting across sessions. Synonyms such as `enable`, `disable`, `true`, `false`, and `--status` are also accepted.

#### line-numbers-replace-prompt

Swap the final line of a multi-line prompt (the row that normally precedes your input) with the line-number gutter. This option only takes effect when line numbers are active and either no custom `PS2` is configured or `cjshopt line-numbers-continuation on` is in effect.

```bash
cjshopt line-numbers-replace-prompt <on|off|status>
```

Examples:

```bash
cjshopt line-numbers-replace-prompt on      # Replace the final prompt line with the numeric gutter
cjshopt line-numbers-replace-prompt off     # Keep the final prompt line visible
cjshopt line-numbers-replace-prompt status  # Show the current setting
```

Add the command to `~/.cjshrc` to persist the setting across sessions.

#### current-line-number-highlight

Enable or disable highlighting of the current line number in multiline input mode. When enabled (default), the line number for the line containing the cursor is displayed in a different style than other line numbers.

```bash
cjshopt current-line-number-highlight <on|off|status>
```

Examples:

```bash
cjshopt current-line-number-highlight on      # Enable current line highlighting
cjshopt current-line-number-highlight off     # Disable current line highlighting
cjshopt current-line-number-highlight status  # Show the current setting
```

Add the command to `~/.cjshrc` to persist the setting across sessions. Accepts synonyms like `enable`, `disable`, `true`, and `false`.

> **Tip:** Customize the current line number style with `cjshopt style_def ic-linenumber-current "bold color=#FFB86C"` to make it stand out from regular line numbers styled with `ic-linenumbers`.

#### hint-delay

Configure the delay (in milliseconds) before inline hints are displayed. This controls how quickly the shell shows suggestions and hints as you type.

```bash
cjshopt hint-delay <milliseconds>
```

Examples:

```bash
cjshopt hint-delay 100     # Set hint delay to 100 milliseconds
cjshopt hint-delay 0       # Show hints immediately
cjshopt hint-delay status  # Show the current delay setting
```

- Valid range: **0 and above** (0 shows hints immediately)
- Default: Varies based on system configuration

Place the command in `~/.cjshrc` to keep the delay setting between sessions.

#### completion-preview

Toggle the completion preview feature, which shows a preview of the selected completion as you navigate through completion options.

```bash
cjshopt completion-preview <on|off|status>
```

Examples:

```bash
cjshopt completion-preview on      # Enable completion preview
cjshopt completion-preview off     # Disable completion preview
cjshopt completion-preview status  # Show the current setting
```

The subcommand accepts synonyms such as `enable`, `disable`, `true`, and `false`. Add to `~/.cjshrc` to persist the preference.

#### visible-whitespace

Show or hide visible markers for whitespace characters (such as spaces) while editing commands. When enabled, spaces are rendered with a subtle middle-dot marker so you can spot trailing or double spacing issues.

```bash
cjshopt visible-whitespace <on|off|status>
```

Examples:

```bash
cjshopt visible-whitespace on      # Show whitespace markers while editing
cjshopt visible-whitespace off     # Hide whitespace markers (default)
cjshopt visible-whitespace status  # Show the current setting
```

Add the command to `~/.cjshrc` to keep the preference across sessions. Synonyms like `enable`, `disable`, `true`, and `false` are accepted.

#### hint

Enable, disable, or inspect inline hints that appear as you type commands. Hints can include suggestions, command completions, and other helpful information.

```bash
cjshopt hint <on|off|status>
```

Examples:

```bash
cjshopt hint on       # Enable inline hints
cjshopt hint off      # Disable inline hints
cjshopt hint status   # Show the current setting
```

Synonyms like `enable`, `disable`, `true`, and `false` are supported. Persist the setting by adding the command to `~/.cjshrc`.

#### multiline-indent

Configure automatic indentation in multiline input mode. When enabled, the shell will automatically indent continuation lines based on the context (e.g., after opening braces, parentheses, or control structures).

```bash
cjshopt multiline-indent <on|off|status>
```

Examples:

```bash
cjshopt multiline-indent on       # Enable automatic indentation
cjshopt multiline-indent off      # Disable automatic indentation
cjshopt multiline-indent status   # Show the current setting
```

This is particularly useful when writing shell scripts or complex commands directly in the shell. Add to `~/.cjshrc` to keep the setting. Accepts synonyms such as `enable`, `disable`, `true`, and `false`.

#### multiline

Enable or disable multiline input mode entirely. When enabled, you can enter commands that span multiple lines. When disabled, the shell treats each line as a separate command.

```bash
cjshopt multiline <on|off|status>
```

Examples:

```bash
cjshopt multiline on       # Enable multiline input
cjshopt multiline off      # Disable multiline input
cjshopt multiline status   # Show the current setting
```

Disabling multiline mode may be useful for simple command execution or when working with scripts that don't require multi-line editing. Accepts synonyms like `enable`, `disable`, `true`, and `false`. Persist by adding to `~/.cjshrc`.

#### inline-help

Toggle inline help messages that appear as you type commands. These messages can provide quick information about command syntax, options, and usage.

```bash
cjshopt inline-help <on|off|status>
```

Examples:

```bash
cjshopt inline-help on       # Enable inline help messages
cjshopt inline-help off      # Disable inline help messages
cjshopt inline-help status   # Show the current setting
```

Supports synonyms such as `enable`, `disable`, `true`, and `false`. Add the command to `~/.cjshrc` to make the setting permanent.

#### auto-tab

Configure automatic tab completion behavior. When enabled, the shell may automatically complete commands or show completions without requiring explicit tab key presses. **Disabled by default.**

```bash
cjshopt auto-tab <on|off|status>
```

Examples:

```bash
cjshopt auto-tab on       # Enable automatic tab completion
cjshopt auto-tab off      # Disable automatic tab completion (default)
cjshopt auto-tab status   # Show the current setting
```

Accepts synonyms including `enable`, `disable`, `true`, and `false`. Place in `~/.cjshrc` to persist the preference across sessions.

#### keybind

Inspect or customize isocline key bindings. Modifying bindings requires running the command from a configuration file (`~/.cjshrc`); runtime changes are only supported for inspection.

```bash
cjshopt keybind <subcommand> [...]
```

Key subcommands include:

- `list` - Show the active profile plus default vs. custom bindings (runtime safe)
- `set <action> <keys...>` - Replace bindings for an action
- `add <action> <keys...>` - Add additional bindings for an action
- `clear <keys...>` - Remove the provided key specifications
- `clear-action <action>` - Remove all custom bindings for an action
- `reset` - Drop every custom binding and restore defaults
- `profile list` - List available key binding profiles (runtime safe)
- `profile set <name>` - Persist the named profile

Key specifications accept pipe (`|`) separated alternatives, so `Ctrl+K|Ctrl+X` is a single argument covering both sequences. Place commands like `cjshopt keybind set cursor-left "Ctrl+H"` in `~/.cjshrc` to keep them between sessions.

#### set-history-max

Adjust the number of entries stored in the persistent history file.

```bash
cjshopt set-history-max <number|default|status>
```

- Provide any non-negative number (0 disables history persistence entirely)
- Use `default` to restore the built-in limit of **1000** entries
- Use `status` (or `--status`) to display the current setting

Examples:

```bash
cjshopt set-history-max 0        # Disable history persistence
cjshopt set-history-max 500      # Retain the latest 500 commands
cjshopt set-history-max default  # Go back to the default limit
cjshopt set-history-max status   # Show the current limit
```

Commands added to `~/.cjshrc` are applied automatically at startup.

#### set-completion-max

Limit how many completion entries are generated and displayed inside the menu each time you
press `Tab`.

```bash
cjshopt set-completion-max <number|default|status>
```

- Provide any number greater than or equal to **1**
- Use `default` to restore the built-in limit of **1000** entries
- Use `status` (or `--status`) to inspect the current setting

Examples:

```bash
cjshopt set-completion-max 50        # Show at most 50 suggestions
cjshopt set-completion-max default   # Restore the default cap
cjshopt set-completion-max status    # Display the current limit
```

Lowering the cap trims visual noise and speeds up completion-heavy commands, especially when
thousands of filesystem matches would otherwise be generated. Add the command to `~/.cjshrc`
to persist the preference.

#### prompt-newline

Control whether cjsh prints a blank line after every command, regardless of what your prompt already emitted.

```bash
cjshopt prompt-newline on
cjshopt prompt-newline off
cjshopt prompt-newline status
```

Enable the toggle to keep transcripts readable when prompts have dense information; disable it for a compact display.

#### prompt-cleanup

Remove the previous prompt before printing command output. This keeps long prompts from duplicating when you scroll back through logs.

```bash
cjshopt prompt-cleanup on
cjshopt prompt-cleanup off
```

Pair this with the newline/empty-line toggles below for additional spacing control.

#### prompt-cleanup-newline

Decide whether prompt cleanup inserts a newline before the next prompt renders.

```bash
cjshopt prompt-cleanup-newline on
cjshopt prompt-cleanup-newline off
```

When enabled, you always get a spacer line between commands once cleanup runs.

#### prompt-cleanup-empty-line

Insert an extra empty line while cleanup runs. This is useful when you want one blank line before and after prompts in transcripts.

```bash
cjshopt prompt-cleanup-empty-line on
cjshopt prompt-cleanup-empty-line off
```

#### prompt-cleanup-truncate

Collapse multiline prompts to a single line after a command has executed so the previous prompt does not dominate your scrollback.

```bash
cjshopt prompt-cleanup-truncate on
cjshopt prompt-cleanup-truncate off
```

All prompt-cleanup toggles accept `status` to report their current state and can be persisted by adding the commands to `~/.cjshrc`.


