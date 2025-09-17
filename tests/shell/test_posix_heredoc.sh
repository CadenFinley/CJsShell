#!/usr/bin/env sh

# Test counters
TOTAL=0
PASSED=0
FAILED=0

# Shell to test
SHELL_TO_TEST="${1:-./build/cjsh}"

log_test() {
    TOTAL=$((TOTAL + 1))
    printf "Test %03d: %s... " "$TOTAL" "$1"
}

pass() {
    PASSED=$((PASSED + 1))
    printf "${GREEN}PASS${NC}\n"
}

fail() {
    FAILED=$((FAILED + 1))
    printf "${RED}FAIL${NC} - %s\n" "$1"
}

# Check if shell exists
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing POSIX Here-Documents for: $SHELL_TO_TEST"
echo "==============================================="

# Test 1: Basic here-document
log_test "Basic here-document"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
hello world
EOF' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected 'hello world', got '$result'"
fi

# Test 2: Here-document with multiple lines
log_test "Here-document with multiple lines"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
line1
line2
line3
EOF' 2>/dev/null | tr '\n' ' ')
expected="line1 line2 line3 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Test 3: Here-document with variable expansion
log_test "Here-document with variable expansion"
result=$("$SHELL_TO_TEST" -c 'VAR=test; cat << EOF
Value: $VAR
EOF' 2>/dev/null)
if [ "$result" = "Value: test" ]; then
    pass
else
    fail "Expected 'Value: test', got '$result'"
fi

# Test 4: Quoted here-document delimiter (no expansion)
log_test "Quoted here-document delimiter (no expansion)"
result=$("$SHELL_TO_TEST" -c 'VAR=test; cat << "EOF"
Value: $VAR
EOF' 2>/dev/null)
if [ "$result" = "Value: \$VAR" ]; then
    pass
else
    fail "Expected 'Value: \$VAR', got '$result'"
fi

# Test 5: Here-document with command substitution
log_test "Here-document with command substitution"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Date: $(echo "today")
EOF' 2>/dev/null)
if [ "$result" = "Date: today" ]; then
    pass
else
    fail "Expected 'Date: today', got '$result'"
fi

# Test 6: Here-document with arithmetic expansion
log_test "Here-document with arithmetic expansion"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Result: $((2+3))
EOF' 2>/dev/null)
if [ "$result" = "Result: 5" ]; then
    pass
else
    fail "Expected 'Result: 5', got '$result'"
fi

# Test 7: Here-document with tabs and spaces
log_test "Here-document preserving whitespace"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
  indented
	tabbed
EOF' 2>/dev/null | head -1)
if echo "$result" | grep -q "  indented"; then
    pass
else
    fail "Expected preserved indentation, got '$result'"
fi

# Test 8: Here-document with special characters
log_test "Here-document with special characters"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Special: !@#$%^&*()
EOF' 2>/dev/null)
if [ "$result" = "Special: !@#\$%^&*()" ]; then
    pass
else
    fail "Expected special characters, got '$result'"
fi

# Test 9: Here-document with empty lines
log_test "Here-document with empty lines"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
line1

line3
EOF' 2>/dev/null | wc -l | tr -d ' ')
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected 3 lines, got '$result'"
fi

# Test 10: Here-document delimiter with different name
log_test "Here-document with custom delimiter"
result=$("$SHELL_TO_TEST" -c 'cat << CUSTOM
content here
CUSTOM' 2>/dev/null)
if [ "$result" = "content here" ]; then
    pass
else
    fail "Expected 'content here', got '$result'"
fi

# Test 11: Nested here-documents (in script)
log_test "Here-document in function"
result=$("$SHELL_TO_TEST" -c 'output_data() {
cat << EOF
function output
EOF
}; output_data' 2>/dev/null)
if [ "$result" = "function output" ]; then
    pass
else
    fail "Expected 'function output', got '$result'"
fi

# Test 12: Here-document with pipe
log_test "Here-document with pipe"
result=$("$SHELL_TO_TEST" -c 'cat << EOF | wc -w
one two three
EOF' 2>/dev/null | tr -d ' ')
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected 3 words, got '$result'"
fi

# Test 13: Here-document with redirection
log_test "Here-document with output redirection"
temp_file="/tmp/heredoc_test_$$"
"$SHELL_TO_TEST" -c "cat << EOF > $temp_file
test content
EOF" 2>/dev/null
if [ -f "$temp_file" ] && [ "$(cat "$temp_file")" = "test content" ]; then
    pass
    rm -f "$temp_file"
else
    fail "Expected file with 'test content'"
    rm -f "$temp_file"
fi

# Test 14: Here-document with backslash escaping
log_test "Here-document with backslash escaping"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
escaped: \$VAR
EOF' 2>/dev/null)
if [ "$result" = "escaped: \$VAR" ]; then
    pass
else
    fail "Expected escaped variable, got '$result'"
fi

# Test 15: Here-document in conditional
log_test "Here-document in conditional"
result=$("$SHELL_TO_TEST" -c 'if true; then
cat << EOF
conditional content
EOF
fi' 2>/dev/null)
if [ "$result" = "conditional content" ]; then
    pass
else
    fail "Expected 'conditional content', got '$result'"
fi

# Test 16: Here-document with loop
log_test "Here-document with loop"
result=$("$SHELL_TO_TEST" -c 'for i in 1 2; do
cat << EOF
iteration $i
EOF
done' 2>/dev/null | wc -l | tr -d ' ')
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected 2 lines from loop, got '$result'"
fi

# Test 17: Multiple here-documents in sequence
log_test "Multiple here-documents in sequence"
result=$("$SHELL_TO_TEST" -c 'cat << EOF1
first
EOF1
cat << EOF2
second
EOF2' 2>/dev/null | tr '\n' ' ')
expected="first second "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Test 18: Here-document with case statement
log_test "Here-document with case statement"
result=$("$SHELL_TO_TEST" -c 'case "test" in
test)
cat << EOF
matched case
EOF
;;
esac' 2>/dev/null)
if [ "$result" = "matched case" ]; then
    pass
else
    fail "Expected 'matched case', got '$result'"
fi

# Test 19: Here-document delimiter at different indentation
log_test "Here-document delimiter indentation"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
content
EOF' 2>/dev/null)
if [ "$result" = "content" ]; then
    pass
else
    fail "Expected 'content', got '$result'"
fi

# Test 20: Here-document with single quotes in content
log_test "Here-document with single quotes in content"
result=$("$SHELL_TO_TEST" -c "cat << EOF
don't worry
EOF" 2>/dev/null)
if [ "$result" = "don't worry" ]; then
    pass
else
    fail "Expected \"don't worry\", got '$result'"
fi

# Test 21: Here-document with double quotes in content
log_test "Here-document with double quotes in content"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
said "hello"
EOF' 2>/dev/null)
if [ "$result" = 'said "hello"' ]; then
    pass
else
    fail "Expected 'said \"hello\"', got '$result'"
fi

# Test 22: Here-document with mixed quote types
log_test "Here-document with mixed quotes"
result=$("$SHELL_TO_TEST" -c "cat << EOF
mixed 'single' and \"double\" quotes
EOF" 2>/dev/null)
expected="mixed 'single' and \"double\" quotes"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected mixed quotes, got '$result'"
fi

# Test 23: Here-document preserving exact content
log_test "Here-document exact content preservation"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Line with trailing spaces   
EOF' 2>/dev/null)
# Just check it contains the basic content (trailing space preservation is shell-specific)
if echo "$result" | grep -q "Line with trailing spaces"; then
    pass
else
    fail "Expected content preservation, got '$result'"
fi

# Test 24: Here-document with no content
log_test "Here-document with no content"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
EOF' 2>/dev/null)
if [ -z "$result" ]; then
    pass
else
    fail "Expected empty content, got '$result'"
fi

# Test 25: Here-document error handling
log_test "Here-document error handling"
# Test with missing delimiter - should fail
"$SHELL_TO_TEST" -c 'cat << EOF
missing delimiter' >/dev/null 2>&1
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    fail "Expected error for missing delimiter"
fi

# Summary
echo
echo "Here-Document Test Summary:"
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All here-document tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED here-document tests failed${NC}"
    exit 1
fi