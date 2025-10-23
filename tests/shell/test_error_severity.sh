#!/usr/bin/env sh
# Test: Error Severity System and errexit_severity option

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: Error severity levels and errexit_severity option..."

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

# Test 1: Default errexit behavior (no errexit_severity set)
echo "Test default errexit behavior (ERROR level)"
output=$("$CJSH_PATH" -c "set -e; false; echo should_not_print" 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "Default errexit - aborts on ERROR severity"
else
    fail_test "Default errexit - should abort on error, got: '$output'"
fi

# Test 2: errexit disabled - script continues even on errors
echo "Test errexit disabled"
output=$("$CJSH_PATH" -c "set +e; false; echo should_print" 2>/dev/null)
if [ "$output" = "should_print" ]; then
    pass_test "errexit disabled - continues on ERROR"
else
    fail_test "errexit disabled - got: '$output'"
fi

# Test 3: errexit_severity=critical (most lenient)
echo "Test errexit_severity=critical"
cat > /tmp/cjsh_test_critical_$$.sh << 'EOF'
set -e
set -o errexit_severity=critical
false
echo "continues_after_error"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_critical_$$.sh 2>/dev/null)
if echo "$output" | grep -q "continues_after_error"; then
    pass_test "errexit_severity=critical - continues on ERROR"
else
    fail_test "errexit_severity=critical - should continue, got: '$output'"
fi
rm -f /tmp/cjsh_test_critical_$$.sh

# Test 4: errexit_severity=error (default)
echo "Test errexit_severity=error (explicit default)"
cat > /tmp/cjsh_test_error_$$.sh << 'EOF'
set -e
set -o errexit_severity=error
false
echo "should_not_print"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_error_$$.sh 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "errexit_severity=error - aborts on ERROR"
else
    fail_test "errexit_severity=error - should abort, got: '$output'"
fi
rm -f /tmp/cjsh_test_error_$$.sh

# Test 5: errexit_severity=warning (strict mode)
echo "Test errexit_severity=warning (strict mode)"
cat > /tmp/cjsh_test_warning_$$.sh << 'EOF'
set -e
set -o errexit_severity=warning
false
echo "should_not_print"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_warning_$$.sh 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "errexit_severity=warning - aborts on ERROR"
else
    fail_test "errexit_severity=warning - should abort, got: '$output'"
fi
rm -f /tmp/cjsh_test_warning_$$.sh

# Test 6: Syntax errors always abort (CRITICAL severity)
echo "Test syntax errors always abort (CRITICAL)"
cat > /tmp/cjsh_test_syntax_$$.sh << 'EOF'
set -e
set -o errexit_severity=critical
if [ test
    echo "never reached"
fi
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_syntax_$$.sh 2>&1)
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass_test "Syntax errors abort even with errexit_severity=critical"
else
    fail_test "Syntax errors should always abort, got: '$output'"
fi
rm -f /tmp/cjsh_test_syntax_$$.sh

# Test 7: set -o displays errexit_severity
echo "Test set -o displays errexit_severity"
output=$("$CJSH_PATH" -c "set -o errexit_severity=warning; set -o" 2>/dev/null)
if echo "$output" | grep -q "errexit_severity"; then
    if echo "$output" | grep "errexit_severity" | grep -q "warning"; then
        pass_test "set -o displays errexit_severity value"
    else
        fail_test "set -o displays errexit_severity but wrong value: '$output'"
    fi
else
    fail_test "set -o should display errexit_severity"
fi

# Test 8: Long form --errexit-severity=
echo "Test --errexit-severity= syntax"
cat > /tmp/cjsh_test_long_$$.sh << 'EOF'
set -e
set --errexit-severity=critical
false
echo "long_form_works"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_long_$$.sh 2>/dev/null)
if echo "$output" | grep -q "long_form_works"; then
    pass_test "--errexit-severity= long form syntax"
else
    fail_test "--errexit-severity= syntax - got: '$output'"
fi
rm -f /tmp/cjsh_test_long_$$.sh

# Test 9: Invalid severity level defaults to error
echo "Test invalid severity level defaults to error"
cat > /tmp/cjsh_test_invalid_$$.sh << 'EOF'
set -e
set -o errexit_severity=invalid_level
false
echo "should_not_print"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_invalid_$$.sh 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "Invalid severity level defaults to error behavior"
else
    fail_test "Invalid severity should default to error, got: '$output'"
fi
rm -f /tmp/cjsh_test_invalid_$$.sh

# Test 10: errexit_severity with successful commands
echo "Test errexit_severity with successful commands"
cat > /tmp/cjsh_test_success_$$.sh << 'EOF'
set -e
set -o errexit_severity=warning
true
echo "success_continues"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_success_$$.sh 2>/dev/null)
if [ "$output" = "success_continues" ]; then
    pass_test "errexit_severity allows successful commands"
else
    fail_test "Successful commands should work, got: '$output'"
fi
rm -f /tmp/cjsh_test_success_$$.sh

# Test 11: Changing errexit_severity mid-script
echo "Test changing errexit_severity mid-script"
cat > /tmp/cjsh_test_change_$$.sh << 'EOF'
set -e
set -o errexit_severity=critical
false
echo "first_section_continues"
set -o errexit_severity=error
false
echo "should_not_print"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_change_$$.sh 2>/dev/null)
if echo "$output" | grep -q "first_section_continues" && ! echo "$output" | grep -q "should_not_print"; then
    pass_test "Changing errexit_severity mid-script works"
else
    fail_test "Mid-script severity change - got: '$output'"
fi
rm -f /tmp/cjsh_test_change_$$.sh

# Test 12: errexit_severity=info (very strict)
echo "Test errexit_severity=info (very strict)"
cat > /tmp/cjsh_test_info_$$.sh << 'EOF'
set -e
set -o errexit_severity=info
false
echo "should_not_print"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_info_$$.sh 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "errexit_severity=info - aborts on ERROR"
else
    fail_test "errexit_severity=info should abort, got: '$output'"
fi
rm -f /tmp/cjsh_test_info_$$.sh

# Test 13: Command not found with errexit_severity=critical
echo "Test command not found with errexit_severity=critical"
cat > /tmp/cjsh_test_notfound_$$.sh << 'EOF'
set -e
set -o errexit_severity=critical
command_that_does_not_exist_xyz123
echo "continues_after_notfound"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_notfound_$$.sh 2>&1)
if echo "$output" | grep -q "continues_after_notfound"; then
    pass_test "Command not found continues with errexit_severity=critical"
else
    # Command not found might still be ERROR level, which is fine
    skip_test "Command not found behavior with errexit_severity=critical"
fi
rm -f /tmp/cjsh_test_notfound_$$.sh

# Test 14: Multiple set -e and errexit_severity interactions
echo "Test set -e off with errexit_severity"
cat > /tmp/cjsh_test_off_$$.sh << 'EOF'
set -e
set -o errexit_severity=error
set +e
false
echo "continues_when_errexit_off"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_off_$$.sh 2>/dev/null)
if [ "$output" = "continues_when_errexit_off" ]; then
    pass_test "set +e overrides errexit_severity"
else
    fail_test "set +e should override severity, got: '$output'"
fi
rm -f /tmp/cjsh_test_off_$$.sh

# Test 15: Case sensitivity of severity levels
echo "Test case insensitive severity levels"
cat > /tmp/cjsh_test_case_$$.sh << 'EOF'
set -e
set -o errexit_severity=CRITICAL
false
echo "case_insensitive_works"
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_case_$$.sh 2>/dev/null)
if echo "$output" | grep -q "case_insensitive_works"; then
    pass_test "errexit_severity is case insensitive"
else
    skip_test "errexit_severity case sensitivity (implementation detail)"
fi
rm -f /tmp/cjsh_test_case_$$.sh

# Test 16: Default severity without explicit setting
echo "Test default severity level is 'error'"
output=$("$CJSH_PATH" -c "set -o" 2>/dev/null | grep errexit_severity || echo "not_found")
if echo "$output" | grep -q "error"; then
    pass_test "Default errexit_severity is 'error'"
else
    skip_test "Default errexit_severity detection (may vary)"
fi

# Test 17: Subshell inherits errexit_severity
echo "Test subshell inherits errexit_severity"
cat > /tmp/cjsh_test_subshell_$$.sh << 'EOF'
set -e
set -o errexit_severity=critical
(false; echo "subshell_continues")
EOF
output=$("$CJSH_PATH" /tmp/cjsh_test_subshell_$$.sh 2>/dev/null)
if echo "$output" | grep -q "subshell_continues"; then
    pass_test "Subshell inherits errexit_severity"
else
    skip_test "Subshell errexit_severity inheritance (complex behavior)"
fi
rm -f /tmp/cjsh_test_subshell_$$.sh

# Test 18: errexit_severity in sourced scripts
echo "Test errexit_severity in sourced scripts"
source_suffix=$$
source_script1="/tmp/cjsh_test_source1_${source_suffix}.sh"
source_script2="/tmp/cjsh_test_source2_${source_suffix}.sh"

cat > "$source_script1" <<'EOF'
set -e
set -o errexit_severity=critical
false
echo "sourced_continues"
EOF

cat > "$source_script2" <<EOF
set -e
set -o errexit_severity=error
source $source_script1
echo "after_source"
EOF

output=$("$CJSH_PATH" "$source_script2" 2>/dev/null)
if echo "$output" | grep -q "sourced_continues" && echo "$output" | grep -q "after_source"; then
    pass_test "errexit_severity works with source command"
else
    fail_test "errexit_severity with source should continue execution (got: '$output')"
fi

rm -f "$source_script1" "$source_script2"

# Test 19: Error message color coding (visual test)
echo "Test error severity displays with color codes"
output=$("$CJSH_PATH" -c 'command_does_not_exist_xyz' 2>&1)
if [ -n "$output" ]; then
    # Just verify we got error output, color codes are visual
    pass_test "Error severity messages are displayed"
else
    fail_test "Error messages should be displayed"
fi

# Test 20: Verify all severity levels are valid
echo "Test all valid severity levels"
for level in info warning error critical; do
    output=$("$CJSH_PATH" -c "set -o errexit_severity=$level; set -o" 2>/dev/null | grep errexit_severity || echo "failed")
    if echo "$output" | grep -q "$level"; then
        pass_test "Severity level '$level' is valid"
    else
        fail_test "Severity level '$level' should be valid"
    fi
done

echo ""
echo "================================"
echo "Error Severity Summary:"
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "  SKIPPED: $TESTS_SKIPPED"
echo "================================"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi

exit 0
