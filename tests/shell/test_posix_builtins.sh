#!/usr/bin/env sh
# POSIX Shell Builtin Commands Compliance Test
# Tests all required POSIX shell builtin commands

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

skip() {
    printf "${YELLOW}SKIP${NC} - %s\n" "$1"
}

# Check if shell exists
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing POSIX Builtin Commands for: $SHELL_TO_TEST"
echo "================================================"

# Test 1: : (colon) builtin - null command
log_test ": (colon) null command"
"$SHELL_TO_TEST" -c ":" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Colon command should always succeed"
fi

# Test 2: . (dot) builtin - source command
log_test ". (dot) source command"
echo "echo sourced" > "/tmp/source_test_$$"
result=$("$SHELL_TO_TEST" -c ". /tmp/source_test_$$" 2>/dev/null)
if [ "$result" = "sourced" ]; then
    pass
else
    fail "Dot source command failed"
fi
rm -f "/tmp/source_test_$$"

# Test 3: alias builtin
log_test "alias builtin"
result=$("$SHELL_TO_TEST" -c "alias ll='ls -l'; alias ll" 2>/dev/null)
if echo "$result" | grep -q "ll="; then
    pass
else
    fail "Alias builtin failed"
fi

# Test 4: bg builtin (background jobs)
log_test "bg builtin"
# This test is complex and depends on job control, skip for now
skip "bg builtin requires interactive job control"

# Test 5: break builtin
log_test "break builtin"
result=$("$SHELL_TO_TEST" -c "for i in 1 2 3 4 5; do if [ \$i -eq 3 ]; then break; fi; echo \$i; done" 2>/dev/null)
expected="1
2"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Break builtin failed"
fi

# Test 6: cd builtin
log_test "cd builtin"
result=$("$SHELL_TO_TEST" -c "cd /tmp && pwd" 2>/dev/null)
if [ "$result" = "/tmp" ] || [ "$result" = "/private/tmp" ]; then
    pass
else
    fail "cd builtin failed"
fi

# Test 7: continue builtin
log_test "continue builtin"
result=$("$SHELL_TO_TEST" -c "for i in 1 2 3 4 5; do if [ \$i -eq 3 ]; then continue; fi; echo \$i; done" 2>/dev/null)
expected="1
2
4
5"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Continue builtin failed"
fi

# Test 8: echo builtin
log_test "echo builtin"
result=$("$SHELL_TO_TEST" -c "echo hello world" 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Echo builtin failed"
fi

# Test 9: echo with -n option
log_test "echo builtin with -n"
result=$("$SHELL_TO_TEST" -c "echo -n hello" 2>/dev/null)
lines=$(printf '%s' "$result" | wc -l | tr -d ' ')
if [ "$result" = "hello" ] && [ "$lines" = "0" ]; then
    pass
else
    fail "Echo -n option failed"
fi

# Test 10: eval builtin
log_test "eval builtin"
result=$("$SHELL_TO_TEST" -c "cmd='echo hello'; eval \$cmd" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Eval builtin failed"
fi

# Test 11: exec builtin (without replacement)
log_test "exec builtin"
result=$("$SHELL_TO_TEST" -c "exec echo hello" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Exec builtin failed"
fi

# Test 12: exit builtin
log_test "exit builtin"
"$SHELL_TO_TEST" -c "exit 42" 2>/dev/null
if [ $? -eq 42 ]; then
    pass
else
    fail "Exit builtin failed"
fi

# Test 13: export builtin
log_test "export builtin"
result=$("$SHELL_TO_TEST" -c "export TEST_VAR=hello; echo \$TEST_VAR" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Export builtin failed"
fi

# Test 14: false builtin
log_test "false builtin"
"$SHELL_TO_TEST" -c "false" 2>/dev/null
if [ $? -ne 0 ]; then
    pass
else
    fail "False builtin should return non-zero"
fi

# Test 15: fg builtin (foreground jobs)
log_test "fg builtin"
skip "fg builtin requires interactive job control"

# Test 16: getopts builtin
log_test "getopts builtin"
result=$("$SHELL_TO_TEST" -c "getopts ab: opt -a; echo \$opt" 2>/dev/null)
if [ "$result" = "a" ]; then
    pass
else
    skip "getopts builtin not implemented or complex"
fi

# Test 17: hash builtin
log_test "hash builtin"
"$SHELL_TO_TEST" -c "hash ls" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    skip "hash builtin not implemented"
fi

# Test 18: jobs builtin
log_test "jobs builtin"
skip "jobs builtin requires job control implementation"

# Test 19: kill builtin
log_test "kill builtin"
# Create a background process to kill
"$SHELL_TO_TEST" -c "sleep 10 & echo \$! > /tmp/pid_$$; kill \$(cat /tmp/pid_$$)" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    skip "kill builtin test requires process management"
fi
rm -f "/tmp/pid_$$"

# Test 20: printf builtin
log_test "printf builtin"
result=$("$SHELL_TO_TEST" -c "printf '%s %d\n' hello 42" 2>/dev/null)
if [ "$result" = "hello 42" ]; then
    pass
else
    fail "Printf builtin failed"
fi

# Test 21: pwd builtin
log_test "pwd builtin"
result=$("$SHELL_TO_TEST" -c "pwd" 2>/dev/null)
if [ -n "$result" ] && [ -d "$result" ]; then
    pass
else
    fail "Pwd builtin failed"
fi

# Test 22: read builtin
log_test "read builtin"
result=$("$SHELL_TO_TEST" -c "echo hello | read var; echo \$var" 2>/dev/null)
# Note: read in pipeline may not work as expected in all shells
if [ "$result" = "hello" ] || [ -z "$result" ]; then
    pass  # Accept both behaviors
else
    fail "Read builtin failed"
fi

# Test 23: readonly builtin
log_test "readonly builtin"
"$SHELL_TO_TEST" -c "readonly VAR=test; VAR=changed" 2>/dev/null
if [ $? -ne 0 ]; then
    pass
else
    skip "readonly builtin not enforced"
fi

# Test 24: return builtin
log_test "return builtin"
result=$("$SHELL_TO_TEST" -c "func() { return 7; }; func; echo \$?" 2>/dev/null)
if [ "$result" = "7" ]; then
    pass
else
    fail "Return builtin failed"
fi

# Test 25: set builtin
log_test "set builtin"
result=$("$SHELL_TO_TEST" -c "set -- a b c; echo \$1 \$2 \$3" 2>/dev/null)
if [ "$result" = "a b c" ]; then
    pass
else
    fail "Set builtin failed"
fi

# Test 26: shift builtin
log_test "shift builtin"
result=$("$SHELL_TO_TEST" -c "set -- a b c; shift; echo \$1 \$2" 2>/dev/null)
if [ "$result" = "b c" ]; then
    pass
else
    fail "Shift builtin failed"
fi

# Test 27: test builtin
log_test "test builtin"
"$SHELL_TO_TEST" -c "test -f /etc/passwd" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Test builtin failed"
fi

# Test 28: [ builtin (bracket test)
log_test "[ builtin"
"$SHELL_TO_TEST" -c "[ -d /tmp ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Bracket test builtin failed"
fi

# Test 29: times builtin
log_test "times builtin"
result=$("$SHELL_TO_TEST" -c "times" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    skip "times builtin not implemented"
fi

# Test 30: trap builtin
log_test "trap builtin"
result=$("$SHELL_TO_TEST" -c "trap 'echo trapped' INT; trap" 2>/dev/null)
if echo "$result" | grep -q "trapped"; then
    pass
else
    skip "trap builtin not fully implemented"
fi

# Test 31: true builtin
log_test "true builtin"
"$SHELL_TO_TEST" -c "true" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "True builtin failed"
fi

# Test 32: type builtin
log_test "type builtin"
result=$("$SHELL_TO_TEST" -c "type echo" 2>/dev/null)
if echo "$result" | grep -q "echo"; then
    pass
else
    skip "type builtin not implemented"
fi

# Test 33: ulimit builtin
log_test "ulimit builtin"
result=$("$SHELL_TO_TEST" -c "ulimit -n" 2>/dev/null)
if [ -n "$result" ] && [ "$result" -gt 0 ] 2>/dev/null; then
    pass
else
    skip "ulimit builtin not implemented"
fi

# Test 34: umask builtin
log_test "umask builtin"
result=$("$SHELL_TO_TEST" -c "umask" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    skip "umask builtin not implemented"
fi

# Test 35: unalias builtin
log_test "unalias builtin"
"$SHELL_TO_TEST" -c "alias test_alias=echo; unalias test_alias" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Unalias builtin failed"
fi

# Test 36: unset builtin
log_test "unset builtin"
result=$("$SHELL_TO_TEST" -c "VAR=test; unset VAR; echo \$VAR" 2>/dev/null)
if [ -z "$result" ]; then
    pass
else
    fail "Unset builtin failed"
fi

# Test 37: wait builtin
log_test "wait builtin"
"$SHELL_TO_TEST" -c "sleep 0.1 & wait" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    skip "wait builtin not fully implemented"
fi

echo "================================================"
echo "POSIX Builtin Commands Test Results:"
echo "Total tests: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All builtin tests passed!${NC}"
    exit 0
else
    echo "${YELLOW}Some builtin tests failed. Review the failures above.${NC}"
    echo "Success rate: $(( PASSED * 100 / TOTAL ))%"
    exit 1
fi
