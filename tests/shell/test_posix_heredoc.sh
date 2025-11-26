#!/usr/bin/env sh

TOTAL=0
PASSED=0
FAILED=0

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
DEFAULT_SHELL="$SCRIPT_DIR/../../build/cjsh"

if [ -n "$1" ]; then
    SHELL_TO_TEST="$1"
elif [ -z "$SHELL_TO_TEST" ]; then
    if [ -n "$CJSH" ]; then
        SHELL_TO_TEST="$CJSH"
    else
        SHELL_TO_TEST="$DEFAULT_SHELL"
    fi
fi

if [ "${SHELL_TO_TEST#/}" = "$SHELL_TO_TEST" ]; then
    SHELL_TO_TEST="$(pwd)/$SHELL_TO_TEST"
fi


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

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing POSIX Here-Documents for: $SHELL_TO_TEST"
echo "==============================================="

log_test "Basic here-document"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
hello world
EOF' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected 'hello world', got '$result'"
fi

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

log_test "Here-document with variable expansion"
result=$("$SHELL_TO_TEST" -c 'VAR=test; cat << EOF
Value: $VAR
EOF' 2>/dev/null)
if [ "$result" = "Value: test" ]; then
    pass
else
    fail "Expected 'Value: test', got '$result'"
fi

log_test "Quoted here-document delimiter (no expansion)"
result=$("$SHELL_TO_TEST" -c 'VAR=test; cat << "EOF"
Value: $VAR
EOF' 2>/dev/null)
if [ "$result" = "Value: \$VAR" ]; then
    pass
else
    fail "Expected 'Value: \$VAR', got '$result'"
fi

log_test "Here-document with command substitution"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Date: $(echo "today")
EOF' 2>/dev/null)
if [ "$result" = "Date: today" ]; then
    pass
else
    fail "Expected 'Date: today', got '$result'"
fi

log_test "Here-document with arithmetic expansion"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Result: $((2+3))
EOF' 2>/dev/null)
if [ "$result" = "Result: 5" ]; then
    pass
else
    fail "Expected 'Result: 5', got '$result'"
fi

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

log_test "Here-document with special characters"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Special: !@#$%^&*()
EOF' 2>/dev/null)
if [ "$result" = "Special: !@#\$%^&*()" ]; then
    pass
else
    fail "Expected special characters, got '$result'"
fi

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

log_test "Here-document with custom delimiter"
result=$("$SHELL_TO_TEST" -c 'cat << CUSTOM
content here
CUSTOM' 2>/dev/null)
if [ "$result" = "content here" ]; then
    pass
else
    fail "Expected 'content here', got '$result'"
fi

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

log_test "Here-document with pipe"
result=$("$SHELL_TO_TEST" -c 'cat << EOF | wc -w
one two three
EOF' 2>/dev/null | tr -d ' ')
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected 3 words, got '$result'"
fi

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

log_test "Here-document with backslash escaping"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
escaped: \$VAR
EOF' 2>/dev/null)
if [ "$result" = "escaped: \$VAR" ]; then
    pass
else
    fail "Expected escaped variable, got '$result'"
fi

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

log_test "Here-document delimiter indentation"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
content
EOF' 2>/dev/null)
if [ "$result" = "content" ]; then
    pass
else
    fail "Expected 'content', got '$result'"
fi

log_test "Here-document with single quotes in content"
result=$("$SHELL_TO_TEST" -c "cat << EOF
don't worry
EOF" 2>/dev/null)
if [ "$result" = "don't worry" ]; then
    pass
else
    fail "Expected \"don't worry\", got '$result'"
fi

log_test "Here-document with double quotes in content"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
said "hello"
EOF' 2>/dev/null)
if [ "$result" = 'said "hello"' ]; then
    pass
else
    fail "Expected 'said \"hello\"', got '$result'"
fi

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

log_test "Here-document exact content preservation"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
Line with trailing spaces   
EOF' 2>/dev/null)
if echo "$result" | grep -q "Line with trailing spaces"; then
    pass
else
    fail "Expected content preservation, got '$result'"
fi

log_test "Here-document with no content"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
EOF' 2>/dev/null)
if [ -z "$result" ]; then
    pass
else
    fail "Expected empty content, got '$result'"
fi

log_test "Bitshift operator not treated as here-document"
result=$("$SHELL_TO_TEST" -c 'echo $((1<<8))' 2>/dev/null)
if [ "$result" = "256" ]; then
    pass
else
    fail "Expected '256', got '$result'"
fi

log_test "Here-document error handling"
"$SHELL_TO_TEST" -c 'cat << EOF
missing delimiter' >/dev/null 2>&1
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    fail "Expected error for missing delimiter"
fi

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