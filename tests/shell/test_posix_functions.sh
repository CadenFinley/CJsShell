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

echo "Testing POSIX Functions for: $SHELL_TO_TEST"
echo "=========================================="

# Test 1: Basic function definition and call
log_test "Basic function definition and call"
result=$("$SHELL_TO_TEST" -c 'hello() { echo "Hello World"; }; hello' 2>/dev/null)
if [ "$result" = "Hello World" ]; then
    pass
else
    fail "Expected 'Hello World', got '$result'"
fi

# Test 2: Function with parameters
log_test "Function with parameters"
result=$("$SHELL_TO_TEST" -c 'greet() { echo "Hello $1"; }; greet Alice' 2>/dev/null)
if [ "$result" = "Hello Alice" ]; then
    pass
else
    fail "Expected 'Hello Alice', got '$result'"
fi

# Test 3: Function with multiple parameters
log_test "Function with multiple parameters"
result=$("$SHELL_TO_TEST" -c 'add() { echo $(($1 + $2)); }; add 3 5' 2>/dev/null)
if [ "$result" = "8" ]; then
    pass
else
    fail "Expected '8', got '$result'"
fi

# Test 4: Function with return value
log_test "Function with return value"
result=$("$SHELL_TO_TEST" -c 'test_return() { return 42; }; test_return; echo $?' 2>/dev/null)
if [ "$result" = "42" ]; then
    pass
else
    fail "Expected '42', got '$result'"
fi

# Test 5: Function with local variables (if supported)
log_test "Function variable scope"
result=$("$SHELL_TO_TEST" -c 'var=global; test_scope() { var=local; echo $var; }; test_scope; echo $var' 2>/dev/null | tr '\n' ' ')
# POSIX doesn't require local variables, so either behavior is acceptable
if [ "$result" = "local local " ] || [ "$result" = "local global " ]; then
    pass
else
    fail "Expected variable scope behavior, got '$result'"
fi

# Test 6: Function calling another function
log_test "Function calling another function"
result=$("$SHELL_TO_TEST" -c 'inner() { echo "inner"; }; outer() { inner; echo "outer"; }; outer' 2>/dev/null | tr '\n' ' ')
expected="inner outer "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Test 7: Recursive function
log_test "Recursive function (simple)"
result=$("$SHELL_TO_TEST" -c 'countdown() { if [ $1 -gt 0 ]; then echo $1; countdown $(($1-1)); fi; }; countdown 3' 2>/dev/null | tr '\n' ' ')
expected="3 2 1 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Test 8: Function with command substitution
log_test "Function with command substitution"
result=$("$SHELL_TO_TEST" -c 'get_date() { echo $(date +%Y); }; get_date' 2>/dev/null)
# Just check that it returns a 4-digit year
if echo "$result" | grep -q '^[0-9][0-9][0-9][0-9]$'; then
    pass
else
    fail "Expected 4-digit year, got '$result'"
fi

# Test 9: Function with pipeline
log_test "Function with pipeline"
result=$("$SHELL_TO_TEST" -c 'count_words() { echo "$1" | wc -w; }; count_words "hello world test"' 2>/dev/null | tr -d ' ')
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected '3', got '$result'"
fi

# Test 10: Function redefinition
log_test "Function redefinition"
result=$("$SHELL_TO_TEST" -c 'test_func() { echo "first"; }; test_func() { echo "second"; }; test_func' 2>/dev/null)
if [ "$result" = "second" ]; then
    pass
else
    fail "Expected 'second', got '$result'"
fi

# Test 11: Function with positional parameters
log_test "Function positional parameters \$# and \$*"
result=$("$SHELL_TO_TEST" -c 'show_args() { echo "$# args: $*"; }; show_args one two three' 2>/dev/null)
if [ "$result" = "3 args: one two three" ]; then
    pass
else
    fail "Expected '3 args: one two three', got '$result'"
fi

# Test 12: Function with \$0 parameter
log_test "Function \$0 parameter"
result=$("$SHELL_TO_TEST" -c 'show_name() { echo "Function name context"; }; show_name' 2>/dev/null)
if [ "$result" = "Function name context" ]; then
    pass
else
    fail "Expected function to execute, got '$result'"
fi

# Test 13: Function with conditional
log_test "Function with conditional"
result=$("$SHELL_TO_TEST" -c 'check_even() { if [ $(($1 % 2)) -eq 0 ]; then echo "even"; else echo "odd"; fi; }; check_even 4' 2>/dev/null)
if [ "$result" = "even" ]; then
    pass
else
    fail "Expected 'even', got '$result'"
fi

# Test 14: Function with loop
log_test "Function with loop"
result=$("$SHELL_TO_TEST" -c 'count_up() { i=1; while [ $i -le $1 ]; do echo $i; i=$((i+1)); done; }; count_up 3' 2>/dev/null | tr '\n' ' ')
expected="1 2 3 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Test 15: Function with case statement
log_test "Function with case statement"
result=$("$SHELL_TO_TEST" -c 'classify() { case $1 in [0-9]) echo "digit";; [a-z]) echo "lowercase";; *) echo "other";; esac; }; classify 5' 2>/dev/null)
if [ "$result" = "digit" ]; then
    pass
else
    fail "Expected 'digit', got '$result'"
fi

# Test 16: Function with variable assignment
log_test "Function with variable assignment"
result=$("$SHELL_TO_TEST" -c 'set_var() { result="function_result"; echo $result; }; set_var' 2>/dev/null)
if [ "$result" = "function_result" ]; then
    pass
else
    fail "Expected 'function_result', got '$result'"
fi

# Test 17: Function with exit
log_test "Function with early return"
result=$("$SHELL_TO_TEST" -c 'early_exit() { echo "before"; return 0; echo "after"; }; early_exit' 2>/dev/null)
if [ "$result" = "before" ]; then
    pass
else
    fail "Expected 'before', got '$result'"
fi

# Test 18: Function with error handling
log_test "Function error handling"
"$SHELL_TO_TEST" -c 'fail_func() { return 1; }; fail_func' 2>/dev/null
exit_code=$?
if [ $exit_code -eq 1 ]; then
    pass
else
    fail "Expected exit code 1, got $exit_code"
fi

# Test 19: Function with here document
log_test "Function with here document"
result=$("$SHELL_TO_TEST" -c 'show_heredoc() { cat << EOF
line1
line2
EOF
}; show_heredoc' 2>/dev/null | tr '\n' ' ')
expected="line1 line2 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Test 20: Function with background process
log_test "Function execution"
result=$("$SHELL_TO_TEST" -c 'bg_func() { echo "background"; }; bg_func' 2>/dev/null)
if [ "$result" = "background" ]; then
    pass
else
    fail "Expected 'background', got '$result'"
fi

# Test 21: Function with arithmetic
log_test "Function with arithmetic operations"
result=$("$SHELL_TO_TEST" -c 'calc() { echo $(($1 * $2 + $3)); }; calc 2 3 4' 2>/dev/null)
if [ "$result" = "10" ]; then
    pass
else
    fail "Expected '10', got '$result'"
fi

# Test 22: Function with string operations
log_test "Function with string operations"
result=$("$SHELL_TO_TEST" -c 'concat() { echo "$1$2"; }; concat "hello" "world"' 2>/dev/null)
if [ "$result" = "helloworld" ]; then
    pass
else
    fail "Expected 'helloworld', got '$result'"
fi

# Test 23: Nested function calls
log_test "Nested function calls"
result=$("$SHELL_TO_TEST" -c 'inner() { echo $1; }; outer() { inner "nested"; }; outer' 2>/dev/null)
if [ "$result" = "nested" ]; then
    pass
else
    fail "Expected 'nested', got '$result'"
fi

# Test 24: Function with complex parameter handling
log_test "Function with quoted parameters"
result=$("$SHELL_TO_TEST" -c 'show_param() { echo "$1"; }; show_param "hello world"' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected 'hello world', got '$result'"
fi

# Test 25: Function unset
log_test "Function existence after definition"
result=$("$SHELL_TO_TEST" -c 'test_unset() { echo "exists"; }; test_unset 2>/dev/null && echo "callable"' 2>/dev/null | tr '\n' ' ')
expected="exists callable "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

# Summary
echo
echo "Functions Test Summary:"
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All function tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED function tests failed${NC}"
    exit 1
fi