# Shell Scripting Guide

CJ's Shell supports comprehensive shell scripting with a POSIX-based core and optional bash-like extensions. This guide covers the essential scripting features.

## Script Basics

### Shebang
Start your scripts with a shebang to specify the interpreter:

```bash
#!/usr/bin/env cjsh
```

Or for POSIX compatibility:
```bash
#!/bin/sh
```

### Making Scripts Executable
```bash
chmod +x script.sh
./script.sh
```

### Running Scripts
```bash
# Execute in a subshell
cjsh script.sh

# Source in current shell (preserves variables and aliases)
source script.sh
. script.sh
```

## Variables

### Variable Assignment
```bash
name="value"
number=42
path="/usr/local/bin"
```

Note: No spaces around the `=` sign.

### Variable Expansion
```bash
echo "$name"
echo "${name}"  # Preferred for clarity
echo "Hello, $name!"
```

### Special Variables
```bash
$0    # Script name
$1-$9 # Positional parameters
$@    # All positional parameters (as separate words)
$*    # All positional parameters (as single word)
$#    # Number of positional parameters
$?    # Exit status of last command
$$    # Process ID of current shell
$!    # Process ID of last background command
```

### Environment Variables
```bash
# Export to make available to child processes
export PATH="/usr/local/bin:$PATH"
export MY_VAR="value"

# Set and export in one line
export NEW_VAR="value"

# Remove variable
unset MY_VAR
```

### Local Variables (in functions)
```bash
function my_func() {
    local temp="temporary value"
    echo "$temp"
}
```

### Read-only Variables
```bash
readonly PI=3.14159
readonly APP_NAME="MyApp"
```

## Conditionals

### if Statements
```bash
if [ condition ]; then
    commands
fi

# With else
if [ condition ]; then
    commands
else
    other_commands
fi

# With elif
if [ condition1 ]; then
    commands1
elif [ condition2 ]; then
    commands2
else
    commands3
fi
```

### Test Expressions

#### String Tests
```bash
[ -z "$str" ]      # True if string is empty
[ -n "$str" ]      # True if string is not empty
[ "$a" = "$b" ]    # True if strings are equal
[ "$a" != "$b" ]   # True if strings are not equal
```

#### Numeric Tests
```bash
[ "$a" -eq "$b" ]  # Equal
[ "$a" -ne "$b" ]  # Not equal
[ "$a" -lt "$b" ]  # Less than
[ "$a" -le "$b" ]  # Less than or equal
[ "$a" -gt "$b" ]  # Greater than
[ "$a" -ge "$b" ]  # Greater than or equal
```

#### File Tests
```bash
[ -e file ]   # True if file exists
[ -f file ]   # True if regular file
[ -d file ]   # True if directory
[ -r file ]   # True if readable
[ -w file ]   # True if writable
[ -x file ]   # True if executable
[ -s file ]   # True if file size > 0
[ -L file ]   # True if symbolic link
```

#### Logical Operators
```bash
[ condition1 ] && [ condition2 ]  # AND
[ condition1 ] || [ condition2 ]  # OR
[ ! condition ]                    # NOT

# Within test
[ condition1 -a condition2 ]  # AND (deprecated, use && instead)
[ condition1 -o condition2 ]  # OR (deprecated, use || instead)
```

### Extended Test [[ ]]
Bash-style extended tests (not POSIX):

```bash
[[ $str =~ pattern ]]     # Regex matching
[[ $str == pattern ]]     # Pattern matching
[[ $a < $b ]]            # String comparison
[[ condition1 && condition2 ]]  # Logical AND
[[ condition1 || condition2 ]]  # Logical OR
```

## Loops

### for Loop
```bash
# Iterate over list
for item in one two three; do
    echo "$item"
done

# Iterate over files
for file in *.txt; do
    echo "Processing $file"
done

# C-style for loop (bash extension)
for ((i=0; i<10; i++)); do
    echo "$i"
done
```

### while Loop
```bash
counter=0
while [ $counter -lt 10 ]; do
    echo "$counter"
    counter=$((counter + 1))
done

# Read file line by line
while IFS= read -r line; do
    echo "$line"
done < file.txt
```

### until Loop
```bash
counter=0
until [ $counter -ge 10 ]; do
    echo "$counter"
    counter=$((counter + 1))
done
```

### Loop Control
```bash
# break - Exit loop
for i in 1 2 3 4 5; do
    if [ $i -eq 3 ]; then
        break
    fi
    echo "$i"
done

# continue - Skip to next iteration
for i in 1 2 3 4 5; do
    if [ $i -eq 3 ]; then
        continue
    fi
    echo "$i"
done
```

## Functions

### Function Definition
```bash
# Method 1 (POSIX)
function_name() {
    commands
}

# Method 2 (bash-style)
function function_name() {
    commands
}
```

### Function Parameters
```bash
greet() {
    local name="$1"
    local greeting="${2:-Hello}"  # Default value
    echo "$greeting, $name!"
}

greet "Alice"              # Hello, Alice!
greet "Bob" "Hi"           # Hi, Bob!
```

### Return Values
```bash
is_valid() {
    if [ -f "$1" ]; then
        return 0  # Success
    else
        return 1  # Failure
    fi
}

# Use in condition
if is_valid "myfile.txt"; then
    echo "File is valid"
fi
```

### Capturing Output
```bash
get_timestamp() {
    date +%Y-%m-%d
}

timestamp=$(get_timestamp)
echo "Current date: $timestamp"
```

## Command Substitution

### Modern Syntax
```bash
result=$(command)
files=$(ls -l)
current_dir=$(pwd)
```

### Nested Substitution
```bash
outer=$(echo "Inner: $(date)")
```

## Arithmetic

### Integer Arithmetic
```bash
# Using $(( ))
result=$((5 + 3))
sum=$((a + b))
product=$((a * b))
division=$((a / b))
modulo=$((a % b))

# Increment/Decrement
count=$((count + 1))
count=$((count - 1))

# Using expr (older style)
result=$(expr 5 + 3)
```

### Common Operations
```bash
# Assignment with arithmetic
((count++))
((count--))
((count += 5))
((count *= 2))

# Comparisons in arithmetic context
if ((a > b)); then
    echo "a is greater"
fi
```

## Input/Output

### Reading Input
```bash
# Read single variable
read name
echo "Hello, $name"

# Read multiple variables
read first last
echo "Name: $first $last"

# With prompt
read -p "Enter your name: " name

# Read into array
read -a array
echo "${array[0]}"

# Read with timeout (bash extension)
read -t 5 -p "Enter within 5 seconds: " input
```

### Printing Output
```bash
# echo
echo "Hello, World"
echo -n "No newline"
echo -e "Enable\tescapes\nhere"

# printf (more portable)
printf "Name: %s\n" "$name"
printf "Number: %d\n" 42
printf "Float: %.2f\n" 3.14159
printf "%-10s %5d\n" "Item" 123
```

## Redirection

### Output Redirection
```bash
command > file           # Redirect stdout, overwrite
command >> file          # Redirect stdout, append
command 2> file          # Redirect stderr
command &> file          # Redirect both stdout and stderr
command 2>&1             # Redirect stderr to stdout
command > file 2>&1      # Redirect both to file
```

### Input Redirection
```bash
command < file           # Read from file
command << EOF           # Here-document
multi-line
input
EOF

command <<< "string"     # Here-string
```

### File Descriptors
```bash
exec 3< input.txt        # Open file for reading
exec 4> output.txt       # Open file for writing
read -u 3 line           # Read from fd 3
echo "text" >&4          # Write to fd 4
exec 3<&-                # Close fd 3
exec 4>&-                # Close fd 4
```

## Pipelines

### Basic Pipelines
```bash
command1 | command2
ls -l | grep ".txt" | wc -l
```

### Pipeline Status
```bash
# Exit status is from last command
ls | grep "pattern"
echo $?  # Status from grep

# Get status of all pipeline commands
ls | grep "pattern"
echo "${PIPESTATUS[@]}"  # bash extension
```

## Case Statements

```bash
case "$variable" in
    pattern1)
        commands1
        ;;
    pattern2|pattern3)
        commands2
        ;;
    *)
        default_commands
        ;;
esac
```

### Example
```bash
case "$1" in
    start)
        echo "Starting..."
        ;;
    stop)
        echo "Stopping..."
        ;;
    restart)
        echo "Restarting..."
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
```

## Error Handling

### Exit on Error
```bash
set -e  # Exit immediately if command fails
set -u  # Exit if undefined variable is used
set -o pipefail  # Pipeline fails if any command fails

# Combine
set -euo pipefail
```

### Custom Error Handling
```bash
command || {
    echo "Command failed" >&2
    exit 1
}

if ! command; then
    echo "Error occurred"
    exit 1
fi
```

### Trap for Cleanup
```bash
cleanup() {
    echo "Cleaning up..."
    rm -f temp_file
}

trap cleanup EXIT
trap 'echo "Interrupted"; exit 1' INT TERM
```

## Arrays (bash extension)

### Array Creation
```bash
array=(one two three)
array[0]="first"
```

### Array Access
```bash
echo "${array[0]}"        # First element
echo "${array[@]}"        # All elements
echo "${#array[@]}"       # Array length
echo "${array[*]}"        # All elements as single word
```

### Array Iteration
```bash
for item in "${array[@]}"; do
    echo "$item"
done
```

## Best Practices

### Quoting
```bash
# Always quote variables
echo "$variable"
command "$file_name"

# Quote to preserve whitespace
for file in "$@"; do
    echo "$file"
done
```

### Script Template
```bash
#!/usr/bin/env cjsh

# Script description
# Author: Your Name
# Date: YYYY-MM-DD

set -euo pipefail

# Constants
readonly SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly SCRIPT_NAME="$(basename "$0")"

# Functions
usage() {
    cat << EOF
Usage: $SCRIPT_NAME [options] arguments

Description of script

Options:
    -h, --help      Show this help message
    -v, --verbose   Enable verbose output

Examples:
    $SCRIPT_NAME file.txt
EOF
}

main() {
    # Main script logic
    echo "Script started"
}

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        *)
            break
            ;;
    esac
done

# Run main
main "$@"
```

### Debugging
```bash
# Enable debugging output
set -x  # Print commands before execution

# Debug specific section
set -x
commands_to_debug
set +x

# Use in script
#!/usr/bin/env cjsh -x
```

### Validation
```bash
# Use syntax command to check script
cjsh syntax script.sh

# Check for specific commands
cjsh validate command_name
```

## Common Patterns

### Argument Parsing
```bash
while getopts "hvf:" opt; do
    case "$opt" in
        h) usage; exit 0 ;;
        v) VERBOSE=1 ;;
        f) FILE="$OPTARG" ;;
        *) usage; exit 1 ;;
    esac
done
shift $((OPTIND - 1))
```

### File Processing
```bash
while IFS= read -r line; do
    # Process each line
    echo "Line: $line"
done < input.txt
```

### Temporary Files
```bash
tmpfile=$(mktemp)
trap "rm -f '$tmpfile'" EXIT

# Use tmpfile
echo "data" > "$tmpfile"
```

### Default Values
```bash
# Use default if variable is unset
name="${name:-default}"

# Set default if variable is unset
: "${name:=default}"

# Error if variable is unset
: "${name:?Error: name is required}"
```

## Additional Resources

- See `help` command for built-in command reference
- Use `man test` for detailed test expression documentation
- Check POSIX specification for portable scripting
- Run `./tests/run_shell_tests.sh` to see example test scripts
