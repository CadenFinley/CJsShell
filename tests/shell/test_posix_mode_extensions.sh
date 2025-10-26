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

echo "Verifying POSIX-mode extension gating for: $SHELL_TO_TEST"
echo "======================================================="

log_test "Brace expansion disabled"
result=$("$SHELL_TO_TEST" --posix -c 'echo {1..3}' 2>/dev/null)
if [ "$result" = "{1..3}" ]; then
    pass
else
    fail "Expected literal brace string, got '$result'"
fi

log_test "Double bracket builtin unavailable"
output=$("$SHELL_TO_TEST" --posix -c '[[ 1 == 1 ]]' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -ne 0 ] && printf "%s" "$output" | grep -qi "command not found"; then
    pass
else
    fail "Expected [[ command rejection (status=$status, output=$clean_output)"
fi

log_test "cjshopt disabled"
output=$("$SHELL_TO_TEST" --posix -c 'cjshopt enable autosuggest' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -ne 0 ] && printf "%s" "$output" | grep -qi "command not found"; then
    pass
else
    fail "Expected cjshopt rejection (status=$status, output=$clean_output)"
fi

log_test "Here-strings rejected"
output=$("$SHELL_TO_TEST" --posix -c 'cat <<< "posix"' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -ne 0 ] && printf "%s" "$output" | grep -qi "syntax error"; then
    pass
else
    fail "Expected here-string syntax error (status=$status, output=$clean_output)"
fi

log_test "Process substitution rejected"
output=$("$SHELL_TO_TEST" --posix -c 'cat <(echo posix)' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -ne 0 ] && printf "%s" "$output" | grep -qi "syntax error"; then
    pass
else
    fail "Expected process substitution syntax error (status=$status, output=$clean_output)"
fi

log_test "function keyword rejected"
output=$("$SHELL_TO_TEST" --posix -c 'function foo { echo hi; }; foo' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -ne 0 ] && printf "%s" "$output" | grep -qi "syntax error"; then
    pass
else
    fail "Expected function keyword error (status=$status, output=$clean_output)"
fi

echo "======================================================="
echo "POSIX Mode Extension Tests Summary:"
echo "Total: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
