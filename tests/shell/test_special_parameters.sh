#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: Special parameters (POSIX compliance gaps)..."
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
echo "Test \$? special parameter"
output=$("$CJSH_PATH" -c "true; echo \$?" 2>/dev/null)
if [ "$output" = "0" ]; then
    pass_test "\$? (exit status) - returns 0 for success"
else
    fail_test "\$? (exit status) - expected 0, got: '$output'"
fi
output=$("$CJSH_PATH" -c "false; echo \$?" 2>/dev/null)
if [ "$output" = "1" ]; then
    pass_test "\$? (exit status) - returns 1 for failure"
else
    fail_test "\$? (exit status) - expected 1, got: '$output'"
fi
echo "Test \$\$ special parameter"
output=$("$CJSH_PATH" -c "echo \$\$" 2>/dev/null)
if [ -n "$output" ] && [ "$output" -gt 0 ] 2>/dev/null; then
    pass_test "\$\$ (process ID) - returns valid PID"
else
    fail_test "\$\$ (process ID) - expected valid PID, got: '$output'"
fi
echo "Test \$! special parameter"
output=$("$CJSH_PATH" -c "sleep 0.1 & echo \$!" 2>/dev/null)
if [ -n "$output" ] && [ "$output" -gt 0 ] 2>/dev/null; then
    pass_test "\$! (background PID) - returns valid PID"
    sleep 0.2
else
    fail_test "\$! (background PID) - expected valid PID, got: '$output'"
fi
echo "Test \$0 special parameter"
output=$("$CJSH_PATH" -c "echo \$0" 2>/dev/null)
if [ -n "$output" ]; then
    pass_test "\$0 (shell name) - returns shell name: '$output'"
else
    fail_test "\$0 (shell name) - expected shell name, got: '$output'"
fi
echo "Test \$1-\$9 positional parameters"
output=$("$CJSH_PATH" -c "set -- one two three; echo \$1 \$2 \$3" 2>/dev/null)
if [ "$output" = "one two three" ]; then
    pass_test "\$1-\$9 (positional parameters)"
else
    fail_test "\$1-\$9 (positional parameters) - expected 'one two three', got: '$output'"
fi
echo "Test \$# special parameter"
output=$("$CJSH_PATH" -c "set -- a b c d e; echo \$#" 2>/dev/null)
if [ "$output" = "5" ]; then
    pass_test "\$# (argument count) - returns correct count"
else
    fail_test "\$# (argument count) - expected 5, got: '$output'"
fi
echo "Test \$* special parameter"
output=$("$CJSH_PATH" -c "set -- one two three; echo \$*" 2>/dev/null)
if [ "$output" = "one two three" ]; then
    pass_test "\$* (all positional parameters)"
else
    fail_test "\$* (all positional parameters) - expected 'one two three', got: '$output'"
fi
echo "Test \$@ special parameter"
output=$("$CJSH_PATH" -c "set -- one two three; echo \$@" 2>/dev/null)
if [ "$output" = "one two three" ]; then
    pass_test "\$@ (all positional parameters)"
else
    fail_test "\$@ (all positional parameters) - expected 'one two three', got: '$output'"
fi
echo "Test quoted \"\$*\" vs \"\$@\" difference"
output=$("$CJSH_PATH" -c 'set -- "arg 1" "arg 2"; for x in "$*"; do echo "[$x]"; done' 2>/dev/null)
lines=$(echo "$output" | wc -l | tr -d ' ')
if [ "$lines" = "1" ]; then
    pass_test "\"\$*\" (quoted) - treats all args as single string"
else
    fail_test "\"\$*\" (quoted) - should be single string, got $lines lines: '$output'"
fi
output=$("$CJSH_PATH" -c 'set -- "arg 1" "arg 2"; for x in "$@"; do echo "[$x]"; done' 2>/dev/null)
lines=$(echo "$output" | wc -l | tr -d ' ')
if [ "$lines" = "2" ]; then
    pass_test "\"\$@\" (quoted) - preserves individual args"
else
    fail_test "\"\$@\" (quoted) - should be 2 args, got $lines lines: '$output'"
fi
echo "Test \$- special parameter (current options)"
output=$("$CJSH_PATH" -c "echo \$-" 2>/dev/null)
if [ -n "$output" ]; then
    pass_test "\$- (current options) - returns option flags: '$output'"
else
    fail_test "\$- (current options) - should return option flags, got empty"
fi
echo "Test \$- reflects set -e option"
output=$("$CJSH_PATH" -c "set -e; echo \$-" 2>/dev/null)
if echo "$output" | grep -q "e"; then
    pass_test "\$- reflects set -e option"
else
    fail_test "\$- should contain 'e' after set -e, got: '$output'"
fi
echo "Test \$_ special parameter (last argument)"
output=$("$CJSH_PATH" -c "echo one two three; echo \$_" 2>/dev/null)
if echo "$output" | grep -q "three"; then
    pass_test "\$_ (last argument) - returns last arg of previous command"
else
    fail_test "\$_ (last argument) - expected to contain 'three', got: '$output'"
fi
echo "Test special parameters in function context"
output=$("$CJSH_PATH" -c 'func() { echo "args: \\$# = \$#, \\$1 = \$1"; }; func one two' 2>/dev/null)
if echo "$output" | grep -q "args: \$# = 2, \$1 = one"; then
    pass_test "Special parameters in function context"
else
    fail_test "Special parameters in function - expected 'args: \$# = 2, \$1 = one', got: '$output'"
fi
echo "Test \$\$ consistency"
output=$("$CJSH_PATH" -c 'pid1=\$\$; sleep 0.01; pid2=\$\$; if [ "\$pid1" = "\$pid2" ]; then echo same; fi' 2>/dev/null)
if [ "$output" = "same" ]; then
    pass_test "\$\$ (process ID) - consistent within script"
else
    fail_test "\$\$ (process ID) - should be consistent, got: '$output'"
fi
echo "Test \$! when no background job"
output=$("$CJSH_PATH" -c "echo \"\$!\"" 2>/dev/null)
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass_test "\$! (no background job) - handles empty case gracefully"
else
    fail_test "\$! (no background job) - should not error"
fi
echo ""
echo "================================"
echo "Special Parameters Summary:"
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "  SKIPPED: $TESTS_SKIPPED"
echo "================================"
if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi
exit 0
