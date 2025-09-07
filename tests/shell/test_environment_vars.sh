#!/usr/bin/env sh
# Test environment variable handling and expansion
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: environment variables..."

# Test basic variable assignment and expansion
OUT=$("$CJSH_PATH" -c "TEST_VAR=hello; echo \$TEST_VAR")
if [ "$OUT" != "hello" ]; then
    echo "FAIL: basic variable assignment (got '$OUT')"
    exit 1
fi

# Test variable with spaces (quoted)
OUT=$("$CJSH_PATH" -c "TEST_VAR='hello world'; echo \$TEST_VAR")
if [ "$OUT" != "hello world" ]; then
    echo "FAIL: variable with spaces (got '$OUT')"
    exit 1
fi

# Test variable concatenation
OUT=$("$CJSH_PATH" -c "A=hello; B=world; echo \${A}\${B}")
if [ "$OUT" != "helloworld" ]; then
    echo "FAIL: variable concatenation (got '$OUT')"
    exit 1
fi

# Test variable in braces
OUT=$("$CJSH_PATH" -c "NAME=test; echo \${NAME}ing")
if [ "$OUT" != "testing" ]; then
    echo "FAIL: variable in braces (got '$OUT')"
    exit 1
fi

# Test undefined variable (should be empty)
OUT=$("$CJSH_PATH" -c "echo [\$UNDEFINED_VAR]")
if [ "$OUT" != "[]" ]; then
    echo "FAIL: undefined variable should be empty (got '$OUT')"
    exit 1
fi

# Test export command
OUT=$("$CJSH_PATH" -c "export TEST_EXPORT=exported; echo \$TEST_EXPORT")
if [ "$OUT" != "exported" ]; then
    echo "FAIL: export command (got '$OUT')"
    exit 1
fi

# Test environment variable inheritance
OUT=$("$CJSH_PATH" -c "export PARENT_VAR=parent; /bin/sh -c 'echo \$PARENT_VAR'")
if [ "$OUT" != "parent" ]; then
    echo "FAIL: environment variable inheritance (got '$OUT')"
    exit 1
fi

# Test special variables
OUT=$("$CJSH_PATH" -c "echo \$\$")
if [ -z "$OUT" ]; then
    echo "FAIL: \$\$ (PID) should have value"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo \$?")
if [ "$OUT" != "0" ]; then
    echo "FAIL: \$? should be 0 after successful command (got '$OUT')"
    exit 1
fi

# Test PATH variable exists
OUT=$("$CJSH_PATH" -c "echo \$PATH")
if [ -z "$OUT" ]; then
    echo "FAIL: PATH variable should be set"
    exit 1
fi

# Test HOME variable exists
OUT=$("$CJSH_PATH" -c "echo \$HOME")
if [ -z "$OUT" ]; then
    echo "FAIL: HOME variable should be set"
    exit 1
fi

# Test USER variable exists
OUT=$("$CJSH_PATH" -c "echo \$USER")
if [ -z "$OUT" ]; then
    echo "FAIL: USER variable should be set"
    exit 1
fi

# Test variable assignment in command line
OUT=$("$CJSH_PATH" -c "TEST_VAR=value echo \$TEST_VAR")
# This should output empty because the variable is only set for the echo command
if [ "$OUT" != "" ]; then
    echo "FAIL: temporary variable assignment (got '$OUT')"
    exit 1
fi

echo "PASS"
exit 0
