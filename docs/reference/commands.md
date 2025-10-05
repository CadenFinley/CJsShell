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
- Smart CD is enabled by default with fuzzy matching and bookmark support

### pwd
Print the current working directory.

```bash
pwd
```

### ls
List directory contents using cjsh's enhanced view.

```bash
ls [options] [path]
```

Note: This is a custom implementation with improved formatting and color support. Can be disabled with `--disable-custom-ls` flag.

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

### syntax
Check scripts or command strings for syntax issues.

```bash
syntax [script_file]
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

### builtin
Run a builtin directly, bypassing functions and PATH.

```bash
builtin command [args...]
```

### validate
Toggle command validation or verify command names.

```bash
validate [on|off|command_name]
```

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
history -c  # Clear history
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

## Theming and Customization

### theme
Manage themes and previews.

```bash
theme [load|list|current] [theme_name]
```

### cjshopt
Generate config files and adjust cjsh options.

```bash
cjshopt <subcommand> [options]
```

Available subcommands:
- `style_def` - Define or redefine syntax highlighting styles
- `login-startup-arg` - Add startup flags (config file only)
- `completion-case` - Configure completion case sensitivity
- `keybind` - Inspect or modify key bindings
- `generate-profile` - Create or overwrite ~/.cjprofile
- `generate-rc` - Create or overwrite ~/.cjshrc
- `generate-logout` - Create or overwrite ~/.cjsh_logout
- `set-max-bookmarks` - Set maximum number of bookmarks
- `set-history-max` - Configure history file size

## Special Internal Commands

### __INTERNAL_SUBSHELL__
Internal command used for subshell execution. Not intended for direct use.
