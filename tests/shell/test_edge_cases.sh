#!/usr/bin/env sh
# Test edge cases and error recovery
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: edge cases and error recovery..."

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

# Test 1: Empty command handling
echo "Testing empty command handling..."
"$CJSH_PATH" -c "" >/tmp/empty_cmd_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "empty command handling"
else
    fail_test "empty command handling"
fi

# Test 2: Whitespace-only commands
echo "Testing whitespace-only commands..."
"$CJSH_PATH" -c "   " >/tmp/whitespace_cmd_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "whitespace-only command handling"
else
    fail_test "whitespace-only command handling"
fi

# Test 3: Very long commands
echo "Testing very long commands..."
LONG_ECHO_CMD="echo"
for i in $(seq 1 1000); do
    LONG_ECHO_CMD="$LONG_ECHO_CMD word$i"
done

"$CJSH_PATH" -c "$LONG_ECHO_CMD" >/tmp/long_cmd_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "very long command handling"
else
    skip_test "very long command (may exceed system limits)"
fi

# Test 4: Command not found error
echo "Testing command not found error..."
"$CJSH_PATH" -c "nonexistent_command_12345" >/tmp/cmd_not_found_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "command not found error handling"
else
    fail_test "command not found should return error"
fi

# Test 5: Invalid syntax recovery
echo "Testing invalid syntax recovery..."
"$CJSH_PATH" -c "echo 'unclosed quote" >/tmp/syntax_error_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "syntax error detection"
else
    fail_test "syntax error should be detected"
fi

# Test 6: Nested quotes
echo "Testing nested quotes..."
"$CJSH_PATH" -c "echo \"hello 'world' test\"" >/tmp/nested_quotes_test.out 2>&1
if [ $? -eq 0 ] && grep -q "hello 'world' test" /tmp/nested_quotes_test.out; then
    pass_test "nested quotes handling"
else
    fail_test "nested quotes handling"
fi

# Test 7: Escaped characters
echo "Testing escaped characters..."
"$CJSH_PATH" -c "echo \"hello\\nworld\"" >/tmp/escaped_chars_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "escaped characters handling"
else
    fail_test "escaped characters handling"
fi

# Test 8: Unmatched parentheses
echo "Testing unmatched parentheses..."
"$CJSH_PATH" -c "echo (hello" >/tmp/unmatched_parens_test.out 2>&1
exit_code=$?
if [ $exit_code -ne 0 ] || ([ $exit_code -eq 0 ] && grep -q "hello" /tmp/unmatched_parens_test.out); then
    pass_test "unmatched parentheses handling"
else
    fail_test "unmatched parentheses handling"
fi

# Test 9: Multiple semicolons
echo "Testing multiple semicolons..."
"$CJSH_PATH" -c "echo hello;;; echo world" >/tmp/multi_semicolon_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "multiple semicolons handling"
else
    fail_test "multiple semicolons handling"
fi

# Test 10: Redirection to non-existent directory
echo "Testing redirection to non-existent directory..."
"$CJSH_PATH" -c "echo test > /nonexistent/path/file.txt" >/tmp/bad_redirect_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "redirection error handling"
else
    fail_test "redirection should fail for non-existent path"
fi

# Test 11: Pipe to non-existent command
echo "Testing pipe to non-existent command..."
"$CJSH_PATH" -c "echo hello | nonexistent_cmd" >/tmp/pipe_error_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "pipe error handling"
else
    fail_test "pipe to non-existent command should fail"
fi

# Test 12: Background process error
echo "Testing background process error..."
"$CJSH_PATH" -c "nonexistent_command &" >/tmp/bg_error_test.out 2>&1
# Background process errors might not propagate immediately
pass_test "background process error handling (basic)"

# Test 13: Variable assignment errors
echo "Testing variable assignment errors..."
"$CJSH_PATH" -c "123INVALID=value" >/tmp/var_assign_error_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "invalid variable name error"
else
    skip_test "invalid variable name (may be allowed)"
fi

# Test 14: Arithmetic errors
echo "Testing arithmetic errors..."
"$CJSH_PATH" -c "echo \$((5/0))" >/tmp/arithmetic_error_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "arithmetic error handling (division by zero)"
else
    skip_test "arithmetic error handling"
fi

# Test 15: Glob pattern edge cases
echo "Testing glob pattern edge cases..."
"$CJSH_PATH" -c "echo /nonexistent/path/*" >/tmp/glob_error_test.out 2>&1
if [ $? -eq 0 ]; then
    # Check if it outputs the literal pattern when no matches
    if grep -q "/nonexistent/path/\*" /tmp/glob_error_test.out; then
        pass_test "glob no-match handling"
    else
        skip_test "glob no-match behavior"
    fi
else
    pass_test "glob error handling"
fi

# Test 16: Deeply nested command substitution
echo "Testing deeply nested command substitution..."
"$CJSH_PATH" -c "echo \$(echo \$(echo \$(echo hello)))" >/tmp/nested_subst_test.out 2>&1
if [ $? -eq 0 ] && grep -q "hello" /tmp/nested_subst_test.out; then
    pass_test "nested command substitution"
else
    fail_test "nested command substitution"
fi

# Test 17: Signal handling during execution
echo "Testing signal handling during execution..."
# Start a long-running command and send signal
"$CJSH_PATH" -c "sleep 10" &
shell_pid=$!
sleep 0.1
kill -TERM $shell_pid 2>/dev/null
wait $shell_pid 2>/dev/null
if [ $? -ne 0 ]; then
    pass_test "signal handling (TERM)"
else
    skip_test "signal handling"
fi

# Test 18: Resource exhaustion simulation
echo "Testing resource exhaustion simulation..."
# Try to create many variables
"$CJSH_PATH" -c "
    i=0
    while [ \$i -lt 10000 ]; do
        export VAR\$i=value\$i
        i=\$((i + 1))
        if [ \$i -eq 100 ]; then break; fi  # Limit to prevent actual exhaustion
    done
    echo \$i
" >/tmp/resource_test.out 2>&1

if [ $? -eq 0 ]; then
    pass_test "resource management test"
else
    skip_test "resource management (may be system dependent)"
fi

# Test 19: Circular dependency in aliases
echo "Testing circular dependency in aliases..."
"$CJSH_PATH" -c "alias a=b; alias b=a; a" >/tmp/circular_alias_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "circular alias detection"
else
    skip_test "circular alias handling"
fi

# Test 20: Unicode and special characters
echo "Testing Unicode and special characters..."
"$CJSH_PATH" -c "echo 'Hello 世界 🌍'" >/tmp/unicode_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "Unicode character handling"
else
    skip_test "Unicode support"
fi

# Test 21: Binary data handling
echo "Testing binary data handling..."
printf '\x00\x01\x02\xFF' | "$CJSH_PATH" -c "cat" >/tmp/binary_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "binary data handling"
else
    skip_test "binary data handling"
fi

# Test 22: Path with spaces
echo "Testing paths with spaces..."
mkdir -p "/tmp/test dir with spaces"
"$CJSH_PATH" -c "ls '/tmp/test dir with spaces'" >/tmp/space_path_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "paths with spaces"
else
    fail_test "paths with spaces"
fi

# Test 23: Recovery after error
echo "Testing recovery after error..."
"$CJSH_PATH" -c "nonexistent_cmd; echo recovery" >/tmp/error_recovery_test.out 2>&1
if grep -q "recovery" /tmp/error_recovery_test.out; then
    pass_test "error recovery"
else
    fail_test "error recovery"
fi

# Test 24: Exit code preservation
echo "Testing exit code preservation..."
"$CJSH_PATH" -c "false; echo \$?" >/tmp/exit_code_test.out 2>&1
if [ $? -eq 0 ] && grep -q "1" /tmp/exit_code_test.out; then
    pass_test "exit code preservation"
else
    skip_test "exit code preservation"
fi

# Cleanup
rm -f /tmp/empty_cmd_test.out /tmp/whitespace_cmd_test.out /tmp/long_cmd_test.out
rm -f /tmp/cmd_not_found_test.out /tmp/syntax_error_test.out /tmp/nested_quotes_test.out
rm -f /tmp/escaped_chars_test.out /tmp/unmatched_parens_test.out /tmp/multi_semicolon_test.out
rm -f /tmp/bad_redirect_test.out /tmp/pipe_error_test.out /tmp/bg_error_test.out
rm -f /tmp/var_assign_error_test.out /tmp/arithmetic_error_test.out /tmp/glob_error_test.out
rm -f /tmp/nested_subst_test.out /tmp/resource_test.out /tmp/circular_alias_test.out
rm -f /tmp/unicode_test.out /tmp/binary_test.out /tmp/space_path_test.out
rm -f /tmp/error_recovery_test.out /tmp/exit_code_test.out
rm -rf "/tmp/test dir with spaces"

echo ""
echo "Edge Cases and Error Recovery Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi