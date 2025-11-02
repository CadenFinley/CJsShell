# Shell Hooks

CJ's Shell provides a lightweight hook system that allows you to execute custom shell functions at key points in the shell's lifecycle. This is similar to Zsh's hook system and enables powerful customizations without modifying the shell's core code.

## Available Hook Types

### `precmd`
Executed before the prompt is displayed, after the previous command has completed.

**Use cases:**
- Update dynamic prompt elements
- Display custom information before each prompt
- Log command execution times
- Update terminal title

**Example:**
```bash
function my_precmd() {
    echo "Last command finished at $(date)"
}
hook add precmd my_precmd
```

### `preexec`
Executed after you press Enter but before the command is actually executed.

**Use cases:**
- Log commands before execution
- Display command start time
- Validate commands before running
- Send notifications for long-running commands

**Example:**
```bash
function my_preexec() {
    echo "About to execute command..."
}
hook add preexec my_preexec
```

### `chpwd`
Executed after successfully changing directories with the `cd` command.

**Use cases:**
- Automatically activate virtual environments
- Display directory-specific information
- Update terminal title with current directory
- Load directory-specific configurations

**Example:**
```bash
function my_chpwd() {
    echo "Changed to: $PWD"
    # Auto-activate Python virtual environment if it exists
    if [ -f ".venv/bin/activate" ]; then
        source .venv/bin/activate
    fi
}
hook add chpwd my_chpwd
```

## Hook Management Commands

### Register a Hook
```bash
hook add <hook_type> <function_name>
```

Registers a shell function to be called at the specified hook point.

**Example:**
```bash
function greet() {
    echo "Hello from hook!"
}
hook add precmd greet
```

### Remove a Hook
```bash
hook remove <hook_type> <function_name>
```

Unregisters a previously registered hook function.

**Example:**
```bash
hook remove precmd greet
```

### List Hooks
```bash
hook list [hook_type]
```

Lists all registered hooks. If `hook_type` is specified, only hooks of that type are shown.

**Examples:**
```bash
# List all hooks
hook list

# List only precmd hooks
hook list precmd
```

### Clear All Hooks
```bash
hook clear <hook_type>
```

Removes all hooks of the specified type.

**Example:**
```bash
hook clear precmd
```

## Complete Examples

### Auto-activate Python Virtual Environments

```bash
# In ~/.cjshrc

function auto_venv() {
    # Deactivate current venv if exists
    if [ ! -z "$VIRTUAL_ENV" ]; then
        deactivate 2>/dev/null
    fi
    
    # Check for virtual environment in current directory
    if [ -f ".venv/bin/activate" ]; then
        source .venv/bin/activate
        echo "✓ Activated Python virtual environment"
    elif [ -f "venv/bin/activate" ]; then
        source venv/bin/activate
        echo "✓ Activated Python virtual environment"
    fi
}

hook add chpwd auto_venv
```

### Display Git Status After Directory Change

```bash
# In ~/.cjshrc

function git_status_on_cd() {
    if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
        echo ""
        git status -sb
        echo ""
    fi
}

hook add chpwd git_status_on_cd
```

### Log All Commands

```bash
# In ~/.cjshrc

function log_command() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $PWD" >> ~/.cjsh_command_log
}

hook add preexec log_command
```

### Display Command Duration

```bash
# In ~/.cjshrc

function command_timer_start() {
    COMMAND_START_TIME=$(date +%s)
}

function command_timer_end() {
    if [ ! -z "$COMMAND_START_TIME" ]; then
        local duration=$(($(date +%s) - COMMAND_START_TIME))
        if [ $duration -gt 5 ]; then
            echo "⏱ Command took ${duration}s"
        fi
        unset COMMAND_START_TIME
    fi
}

hook add preexec command_timer_start
hook add precmd command_timer_end
```

### Dynamic Terminal Title

```bash
# In ~/.cjshrc

function update_terminal_title() {
    # Set terminal title to current directory
    echo -ne "\033]0;${PWD/#$HOME/~}\007"
}

hook add precmd update_terminal_title
hook add chpwd update_terminal_title
```

## Best Practices

1. **Keep hooks fast**: Hooks are executed synchronously and can slow down your shell if they take too long.

2. **Handle errors gracefully**: Hooks should not exit with error codes that affect subsequent commands.

3. **Use functions**: Define hooks as shell functions for better organization and reusability.

4. **Avoid infinite loops**: Be careful with hooks that might trigger themselves (e.g., `chpwd` that calls `cd`).

5. **Test hooks**: Test your hooks in a separate shell session before adding them to `~/.cjshrc`.

6. **Document your hooks**: Add comments to your `~/.cjshrc` explaining what each hook does.

## Debugging Hooks

To debug hook execution, you can temporarily add debugging output:

```bash
function my_hook() {
    echo "DEBUG: my_hook called" >&2
    # Your actual hook code here
}
```

To disable a problematic hook without editing files:

```bash
# In the shell
hook remove precmd problematic_function
```

## Hook Execution Order

When multiple hooks of the same type are registered, they are executed in the order they were registered:

```bash
hook add precmd first_function
hook add precmd second_function
hook add precmd third_function

# Execution order: first_function, second_function, third_function
```

## Differences from Zsh Hooks

CJ's Shell hooks are inspired by Zsh but have some differences:

- **Simpler API**: Uses a single `hook` command instead of `add-zsh-hook`
- **Fewer hook types**: Currently supports only `precmd`, `preexec`, and `chpwd`
- **No automatic unhooking**: Functions are not automatically removed when undefined
- **Manual management**: No automatic hook discovery from function names

## Configuration File Example

Here's a complete example of hook usage in `~/.cjshrc`:

```bash
# ~/.cjshrc - CJ's Shell Configuration with Hooks

# Function definitions
function show_git_info() {
    if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
        local branch=$(git branch --show-current)
        echo "Git branch: $branch"
    fi
}

function greet_directory() {
    local dir=$(basename "$PWD")
    echo "→ Entered: $dir"
}

function check_todos() {
    if [ -f "TODO.md" ]; then
        echo "This directory has a TODO.md file"
    fi
}

# Register hooks
hook add chpwd show_git_info
hook add chpwd greet_directory
hook add chpwd check_todos

echo "Hooks loaded successfully"
```

## See Also

- [Built-in Commands Reference](commands.md)
- [Configuration Guide](../getting-started/quick-start.md#configuration)
