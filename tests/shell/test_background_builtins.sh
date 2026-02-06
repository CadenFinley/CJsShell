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

echo "Testing background builtin behavior for: $SHELL_TO_TEST"
echo "======================================================"

log_test "Background true builtin does not error"
output=$("$SHELL_TO_TEST" -c 'true & jobs' 2>&1)
if printf '%s' "$output" | grep -q "command not found" || printf '%s' "$output" | grep -q "Exit 127"; then
    fail "true & should not error (output=$output)"
else
    pass
fi

log_test "Background false builtin does not error"
output=$("$SHELL_TO_TEST" -c 'false & jobs' 2>&1)
if printf '%s' "$output" | grep -q "command not found" || printf '%s' "$output" | grep -q "Exit 127"; then
    fail "false & should not error (output=$output)"
else
    pass
fi

log_test "Background cd does not affect parent"
result=$("$SHELL_TO_TEST" -c 'orig=$(pwd); cd / & sleep 0.05; after=$(pwd); if [ "$orig" = "$after" ]; then echo ok; else echo fail:$orig:$after; fi' 2>/dev/null)
if [ "$result" = "ok" ]; then
    pass
else
    fail "cd in background should not change parent (result=$result)"
fi

log_test "Background export does not leak"
result=$("$SHELL_TO_TEST" -c 'unset CJSH_BG_EXPORT_TEST; export CJSH_BG_EXPORT_TEST=1 & sleep 0.05; if [ -z "$CJSH_BG_EXPORT_TEST" ]; then echo ok; else echo fail:$CJSH_BG_EXPORT_TEST; fi' 2>/dev/null)
if [ "$result" = "ok" ]; then
    pass
else
    fail "export in background should not leak (result=$result)"
fi

log_test "Background alias does not leak"
result=$("$SHELL_TO_TEST" -c 'unalias cjsh_bg_alias 2>/dev/null; alias cjsh_bg_alias="echo hi" & sleep 0.05; alias cjsh_bg_alias >/dev/null 2>&1; alias_status=$?; if [ $alias_status -ne 0 ]; then echo ok; else echo fail:$alias_status; fi' 2>/dev/null)
if [ "$result" = "ok" ]; then
    pass
else
    fail "alias in background should not leak (result=$result)"
fi

log_test "Background set does not affect positional params"
result=$("$SHELL_TO_TEST" -c 'set -- one two; set -- three four & sleep 0.05; echo "$1 $2"' 2>/dev/null)
if [ "$result" = "one two" ]; then
    pass
else
    fail "set in background should not change parent args (got '$result')"
fi

echo ""
echo "Background Builtin Test Results:"
echo "==============================="
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All background builtin tests passed!${NC}"
    exit 0
else
    echo "${RED}Some background builtin tests failed.${NC}"
    exit 1
fi
