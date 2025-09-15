#!/usr/bin/env sh
# Test environment variable handling and expansion
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: environment variables..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

# Test basic variable assignment and expansion
OUT=$("$CJSH_PATH" -c "TEST_VAR=hello; echo \$TEST_VAR")
if [ "$OUT" = "hello" ]; then
    pass_test "basic variable assignment"
else
    fail_test "basic variable assignment (got '$OUT')"
fi

# Test variable with spaces (quoted)
OUT=$("$CJSH_PATH" -c "TEST_VAR='hello world'; echo \$TEST_VAR")
if [ "$OUT" = "hello world" ]; then
    pass_test "variable with spaces"
else
    fail_test "variable with spaces (got '$OUT')"
fi

# Test variable concatenation
OUT=$("$CJSH_PATH" -c "A=hello; B=world; echo \${A}\${B}")
if [ "$OUT" = "helloworld" ]; then
    pass_test "variable concatenation"
else
    fail_test "variable concatenation (got '$OUT')"
fi

# Test variable in braces
OUT=$("$CJSH_PATH" -c "NAME=test; echo \${NAME}ing")
if [ "$OUT" = "testing" ]; then
    pass_test "variable in braces"
else
    fail_test "variable in braces (got '$OUT')"
fi

# Test undefined variable (should be empty)
OUT=$("$CJSH_PATH" -c "echo [\$UNDEFINED_VAR]")
if [ "$OUT" = "[]" ]; then
    pass_test "undefined variable should be empty"
else
    fail_test "undefined variable should be empty (got '$OUT')"
fi

# Test export command
OUT=$("$CJSH_PATH" -c "export TEST_EXPORT=exported; echo \$TEST_EXPORT")
if [ "$OUT" = "exported" ]; then
    pass_test "export command"
else
    fail_test "export command (got '$OUT')"
fi

# Test environment variable inheritance
OUT=$("$CJSH_PATH" -c "export PARENT_VAR=parent; /bin/sh -c 'echo \$PARENT_VAR'")
if [ "$OUT" = "parent" ]; then
    pass_test "environment variable inheritance"
else
    fail_test "environment variable inheritance (got '$OUT')"
fi

# Test special variables
OUT=$("$CJSH_PATH" -c "echo \$\$")
if [ -n "$OUT" ]; then
    pass_test "\$\$ (PID) has value"
else
    fail_test "\$\$ (PID) should have value"
fi

OUT=$("$CJSH_PATH" -c "echo \$?")
if [ "$OUT" = "0" ]; then
    pass_test "\$? should be 0 after successful command"
else
    fail_test "\$? should be 0 after successful command (got '$OUT')"
fi

# Test PATH variable exists
OUT=$("$CJSH_PATH" -c "echo \$PATH")
if [ -n "$OUT" ]; then
    pass_test "PATH variable should be set"
else
    fail_test "PATH variable should be set"
fi

# Test HOME variable exists
OUT=$("$CJSH_PATH" -c "echo \$HOME")
if [ -n "$OUT" ]; then
    pass_test "HOME variable should be set"
else
    fail_test "HOME variable should be set"
fi

# Test USER variable exists
OUT=$("$CJSH_PATH" -c "echo \$USER")
if [ -n "$OUT" ]; then
    pass_test "USER variable should be set"
else
    fail_test "USER variable should be set"
fi

# Test variable assignment in command line
OUT=$("$CJSH_PATH" -c "TEST_VAR=value echo \$TEST_VAR")
# This should output empty because the variable is only set for the echo command
if [ "$OUT" = "" ]; then
    pass_test "temporary variable assignment"
else
    fail_test "temporary variable assignment (got '$OUT')"
fi

# Summary
echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))

if [ $TESTS_FAILED -eq 0 ] && [ $TESTS_SKIPPED -eq 0 ]; then
    printf "${GREEN}All tests passed! ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    exit 0
elif [ $TESTS_FAILED -eq 0 ]; then
    printf "${YELLOW}All tests passed with some skipped. ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    exit 0
else
    printf "${RED}Some tests failed. ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    printf "Passed: ${GREEN}$TESTS_PASSED${NC}, Failed: ${RED}$TESTS_FAILED${NC}, Skipped: ${YELLOW}$TESTS_SKIPPED${NC}\n"
    exit 1
fi
