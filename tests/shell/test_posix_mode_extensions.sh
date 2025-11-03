#!/usr/bin/env sh

TOTAL=0
PASSED=0
FAILED=0

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

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Checking interactive behavior without POSIX gating for: $SHELL_TO_TEST"
echo "======================================================="

log_test "--posix flag rejected"
output=$("$SHELL_TO_TEST" --posix -c 'echo hi' 2>&1)
status=$?
if [ $status -eq 127 ]; then
    pass
else
    clean_output=$(printf "%s" "$output" | tr '\n' ' ')
    fail "Expected exit status 127 for unknown flag (status=$status, output=$clean_output)"
fi

log_test "Brace expansion active"
result=$("$SHELL_TO_TEST" -c 'echo {1..3}' 2>/dev/null)
if [ "$result" = "1 2 3" ]; then
    pass
else
    fail "Expected '1 2 3', got '$result'"
fi

log_test "Double bracket builtin available"
"$SHELL_TO_TEST" -c '[[ 1 == 1 ]]' >/dev/null 2>&1
status=$?
if [ $status -eq 0 ]; then
    pass
else
    fail "[[ ]] should succeed (status=$status)"
fi

log_test "cjshopt available"
output=$("$SHELL_TO_TEST" -c 'cjshopt completion-case status' 2>&1)
status=$?
if [ $status -eq 0 ]; then
    pass
else
    clean_output=$(printf "%s" "$output" | tr '\n' ' ')
    fail "cjshopt command failed (status=$status, output=$clean_output)"
fi

log_test "Here-strings supported"
output=$("$SHELL_TO_TEST" -c 'cat <<< "posix"' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -eq 0 ] && [ "$clean_output" = "posix" ]; then
    pass
else
    fail "Expected here-string to output 'posix' (status=$status, output=$clean_output)"
fi

log_test "Process substitution available"
output=$("$SHELL_TO_TEST" -c 'cat <(echo posix)' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -eq 0 ] && [ "$clean_output" = "posix" ]; then
    pass
else
    fail "Expected process substitution to output 'posix' (status=$status, output=$clean_output)"
fi

log_test "function keyword allowed"
output=$("$SHELL_TO_TEST" -c 'function foo { echo hi; }; foo' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -eq 0 ] && [ "$clean_output" = "hi" ]; then
    pass
else
    fail "Expected function keyword to execute (status=$status, output=$clean_output)"
fi

echo "======================================================="
echo "Interactive Mode Tests Summary:"
echo "Total: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
