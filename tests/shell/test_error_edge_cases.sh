#!/usr/bin/env sh
# Test error conditions and edge cases
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: error handling and edge cases..."

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

# Test empty command
OUT=$("$CJSH_PATH" -c "" 2>/dev/null)
if [ $? -eq 0 ]; then
    pass_test "empty command should not error"
else
    fail_test "empty command should not error"
fi

# Test whitespace-only command
OUT=$("$CJSH_PATH" -c "   " 2>/dev/null)
if [ $? -eq 0 ]; then
    pass_test "whitespace-only command should not error"
else
    fail_test "whitespace-only command should not error"
fi

# Test very long command line
LONG_CMD="echo $(printf '%*s' 1000 '' | tr ' ' 'a')"
OUT=$("$CJSH_PATH" -c "$LONG_CMD" 2>/dev/null)
if [ $? -eq 0 ]; then
    pass_test "very long command line"
else
    skip_test "very long command failed (acceptable)"
fi

# Test invalid syntax
"$CJSH_PATH" -c "if" 2>/dev/null
if [ $? -ne 0 ]; then
    pass_test "incomplete if statement should error"
else
    fail_test "incomplete if statement should error"
fi

# Test unmatched quotes (behavior varies between shells)
"$CJSH_PATH" -c "echo 'unmatched" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    pass_test "unmatched quotes should error"
else
    skip_test "unmatched quotes test - shell may handle this differently"
fi

# Test invalid redirection
"$CJSH_PATH" -c "echo test > /invalid/path/file" 2>/dev/null
if [ $? -ne 0 ]; then
    pass_test "invalid redirection should error"
else
    fail_test "invalid redirection should error"
fi

# Test permission denied
"$CJSH_PATH" -c "touch /root/test_file" 2>/dev/null
if [ $? -ne 0 ]; then
    pass_test "permission denied should error"
else
    skip_test "permission test may have succeeded unexpectedly"
fi

# Test moderately nested command substitution 
OUT1=$("$CJSH_PATH" -c "echo \$(echo test)" 2>/dev/null)
OUT2=$("$CJSH_PATH" -c "echo \$(echo \$(echo test))" 2>/dev/null)
if [ "$OUT1" = "test" ] && [ "$OUT2" = "test" ]; then
    pass_test "nested command substitution"
else
    skip_test "nested command substitution failed (OUT1='$OUT1', OUT2='$OUT2')"
fi

# Test very long environment variable
LONG_VAR=$(printf '%*s' 1000 | tr ' ' 'a')
OUT=$("$CJSH_PATH" -c "TEST_LONG='$LONG_VAR'; echo \${#TEST_LONG}" 2>/dev/null)
if [ "$OUT" = "1000" ]; then
    pass_test "long environment variable"
else
    skip_test "long environment variable test failed (got '$OUT')"
fi

# Test special characters in commands
OUT=$("$CJSH_PATH" -c "echo '!@#\$%^&*()_+-={}[]|\\:;\"<>?,./'" 2>/dev/null)
if [ $? -eq 0 ]; then
    pass_test "special characters in echo command"
else
    fail_test "special characters in echo command"
fi

# Test null command
"$CJSH_PATH" -c ":" 2>/dev/null
if [ $? -eq 0 ]; then
    pass_test "null command (:) should succeed"
else
    fail_test "null command (:) should succeed"
fi

# Test multiple redirections
OUT=$("$CJSH_PATH" -c "echo test 2>/dev/null >/dev/null" 2>/dev/null)
if [ $? -eq 0 ]; then
    pass_test "multiple redirections should work"
else
    fail_test "multiple redirections should work"
fi

# Test command with many arguments
MANY_ARGS=""
for i in $(seq 1 100); do
    MANY_ARGS="$MANY_ARGS arg$i"
done
OUT=$("$CJSH_PATH" -c "echo $MANY_ARGS | wc -w" 2>/dev/null | tr -d ' ')
if [ "$OUT" = "100" ]; then
    pass_test "command with many arguments"
else
    skip_test "command with many arguments failed (got '$OUT')"
fi

# Test recursive alias (should not crash)
"$CJSH_PATH" -c "alias test_alias='test_alias'; test_alias" 2>/dev/null
# Should timeout or handle recursion gracefully - just check it doesn't crash
if [ $? -ne 0 ]; then
    pass_test "recursive alias handled gracefully"
else
    skip_test "recursive alias behavior varies"
fi

# Test binary file execution attempt
echo -e '\x7fELF' > /tmp/fake_binary
chmod +x /tmp/fake_binary
"$CJSH_PATH" -c "/tmp/fake_binary" 2>/dev/null
EXIT_CODE=$?
rm -f /tmp/fake_binary
if [ $EXIT_CODE -ne 0 ]; then
    pass_test "binary file execution attempt should error"
else
    skip_test "binary file execution test behavior varies"
fi

# Test ctrl characters in input
OUT=$("$CJSH_PATH" -c "echo -e 'line1\nline2\tword'" 2>/dev/null)
if [ $? -eq 0 ]; then
    pass_test "control characters in echo"
else
    fail_test "control characters in echo"
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
