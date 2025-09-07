#!/usr/bin/env sh
# Compare CJ's Shell against known POSIX-compliant shells

CJSH="./build/cjsh"
SHELLS="sh dash bash"  # Test against multiple shells

echo "Comparing shell behaviors..."
echo "=========================="

test_command() {
    local cmd="$1"
    local desc="$2"
    
    echo ""
    echo "Test: $desc"
    echo "Command: $cmd"
    echo "---"
    
    for shell in $CJSH $SHELLS; do
        if command -v "$shell" >/dev/null 2>&1 || [ -x "$shell" ]; then
            echo -n "$(basename "$shell"): "
            result=$("$shell" -c "$cmd" 2>&1)
            exit_code=$?
            echo "[$exit_code] '$result'"
        fi
    done
}

# Test basic functionality
test_command "echo hello" "Basic echo"

# Test variable expansion
test_command 'var=test; echo $var' "Variable expansion"

# Test command substitution
test_command 'echo $(echo nested)' "Command substitution"

# Test pipeline
test_command 'echo hello | cat' "Simple pipeline"

# Test here document (problematic)
test_command 'cat << EOF
line1
line2
EOF' "Here document"

# Test error redirection (problematic)
test_command 'echo error >&2 2>/dev/null; echo done' "Error redirection"

# Test logical operators
test_command 'true && echo success' "Logical AND"
test_command 'false || echo success' "Logical OR"

# Test file test operators
test_command '[ -f /etc/passwd ] && echo exists' "File test"

echo ""
echo "=========================="
echo "Comparison complete."
