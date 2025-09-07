#!/usr/bin/env sh
# Test process management and job control
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: process management..."

# Test basic command execution and exit status
"$CJSH_PATH" -c "true"
if [ $? -ne 0 ]; then
    echo "FAIL: true command should exit with 0"
    exit 1
fi

"$CJSH_PATH" -c "false"
if [ $? -ne 1 ]; then
    echo "FAIL: false command should exit with 1"
    exit 1
fi

# Test exit status propagation
OUT=$("$CJSH_PATH" -c "false; echo \$?")
if [ "$OUT" != "1" ]; then
    echo "FAIL: exit status propagation (got '$OUT')"
    exit 1
fi

# Test process substitution
OUT=$("$CJSH_PATH" -c "echo hello | wc -w" | tr -d ' ')
if [ "$OUT" != "1" ]; then
    echo "FAIL: process substitution (got '$OUT')"
    exit 1
fi

# Test multiple commands with ;
OUT=$("$CJSH_PATH" -c "echo first; echo second")
EXPECTED="first
second"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: multiple commands with semicolon (got '$OUT')"
    exit 1
fi

# Test background process (basic test)
"$CJSH_PATH" -c "sleep 0.1 &"
if [ $? -ne 0 ]; then
    echo "FAIL: background process execution"
    exit 1
fi

# Test command not found
"$CJSH_PATH" -c "nonexistent_command_12345" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "FAIL: nonexistent command should return non-zero"
    exit 1
fi

# Test signal handling (basic)
# We can't easily test interactive signals, but we can test that
# the shell doesn't crash on basic signal scenarios

# Test timeout handling (if supported)
timeout 1 "$CJSH_PATH" -c "sleep 2" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    echo "FAIL: timeout should interrupt long-running command"
    exit 1
fi

# Test exec replacement
OUT=$("$CJSH_PATH" -c "exec echo 'exec test'")
if [ "$OUT" != "exec test" ]; then
    echo "FAIL: exec command (got '$OUT')"
    exit 1
fi

# Test exit builtin
"$CJSH_PATH" -c "exit 42"
if [ $? -ne 42 ]; then
    echo "FAIL: exit builtin with custom code"
    exit 1
fi

# Test process environment inheritance
export TEST_PROC_VAR="inherited"
OUT=$("$CJSH_PATH" -c "echo \$TEST_PROC_VAR")
if [ "$OUT" != "inherited" ]; then
    echo "FAIL: process environment inheritance (got '$OUT')"
    exit 1
fi

# Test command substitution in process context
OUT=$("$CJSH_PATH" -c "echo \$(echo substituted)")
if [ "$OUT" != "substituted" ]; then
    echo "FAIL: command substitution in process (got '$OUT')"
    exit 1
fi

# Test nested command execution
OUT=$("$CJSH_PATH" -c "echo \$(echo \$(echo nested))")
if [ "$OUT" != "nested" ]; then
    echo "FAIL: nested command execution (got '$OUT')"
    exit 1
fi

# Test PATH resolution
OUT=$("$CJSH_PATH" -c "which echo")
if [ -z "$OUT" ]; then
    echo "FAIL: PATH resolution for which command"
    exit 1
fi

echo "PASS"
exit 0
