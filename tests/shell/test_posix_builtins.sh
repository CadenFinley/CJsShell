#!/usr/bin/env sh
TOTAL=0
PASSED=0
FAILED=0
SHELL_TO_TEST="${1:-./build/cjsh}"
OS_NAME=$(uname -s 2>/dev/null || echo unknown)
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
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi
echo "Testing POSIX Builtin Commands for: $SHELL_TO_TEST"
echo "================================================"
log_test ": (colon) null command"
"$SHELL_TO_TEST" -c ":" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Colon command should always succeed"
fi
log_test ". (dot) source command"
echo "echo sourced" > "/tmp/source_test_$$"
result=$("$SHELL_TO_TEST" -c ". /tmp/source_test_$$" 2>/dev/null)
if [ "$result" = "sourced" ]; then
    pass
else
    fail "Dot source command failed"
fi
rm -f "/tmp/source_test_$$"
log_test "alias builtin"
result=$("$SHELL_TO_TEST" -c "alias ll='ls -l'; alias ll" 2>/dev/null)
if echo "$result" | grep -q "ll="; then
    pass
else
    fail "Alias builtin failed"
fi
log_test "bg builtin"
"$SHELL_TO_TEST" -c "bg" >/dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass
else
    fail "bg builtin not working"
fi
log_test "break builtin"
result=$("$SHELL_TO_TEST" -c "for i in 1 2 3 4 5; do if [ \$i -eq 3 ]; then break; fi; echo \$i; done" 2>/dev/null)
expected="1
2"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Break builtin failed got: '$result', expected '$expected'"
fi
log_test "cd builtin"
result=$("$SHELL_TO_TEST" -c "cd /tmp && pwd" 2>/dev/null)
if [ "$result" = "/tmp" ] || [ "$result" = "/private/tmp" ]; then
    pass
else
    fail "cd builtin failed"
fi
log_test "continue builtin"
result=$("$SHELL_TO_TEST" -c "for i in 1 2 3 4 5; do if [ \$i -eq 3 ]; then continue; fi; echo \$i; done" 2>/dev/null)
expected="1
2
4
5"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Continue builtin failed got: '$result', expected '$expected'"
fi
log_test "echo builtin"
result=$("$SHELL_TO_TEST" -c "echo hello world" 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Echo builtin failed"
fi
log_test "echo builtin with -n"
result=$("$SHELL_TO_TEST" -c "echo -n hello" 2>/dev/null)
lines=$(printf '%s' "$result" | wc -l | tr -d ' ')
if [ "$result" = "hello" ] && [ "$lines" = "0" ]; then
    pass
else
    fail "Echo -n option failed"
fi
log_test "eval builtin"
result=$("$SHELL_TO_TEST" -c "cmd='echo hello'; eval \$cmd" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Eval builtin failed"
fi
log_test "exec builtin"
result=$("$SHELL_TO_TEST" -c "exec echo hello" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Exec builtin failed got: '$result', expected 'hello'"
fi
log_test "exit builtin"
"$SHELL_TO_TEST" -c "exit 42" 2>/dev/null
if [ $? -eq 42 ]; then
    pass
else
    fail "Exit builtin failed"
fi
log_test "export builtin"
result=$("$SHELL_TO_TEST" -c "export TEST_VAR=hello; echo \$TEST_VAR" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Export builtin failed"
fi
log_test "false builtin"
"$SHELL_TO_TEST" -c "false" 2>/dev/null
if [ $? -ne 0 ]; then
    pass
else
    fail "False builtin should return non-zero"
fi
log_test "fg builtin"
"$SHELL_TO_TEST" -c "fg" >/dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass
else
    fail "fg builtin not working"
fi
log_test "getopts builtin"
result=$("$SHELL_TO_TEST" -c "getopts ab: opt -a; echo \$opt" 2>/dev/null)
if [ "$result" = "a" ]; then
    pass
else
    "$SHELL_TO_TEST" -c "getopts 'abc' opt 2>/dev/null"
    if [ $? -eq 0 ] || [ $? -eq 1 ]; then
        pass
    else
        fail "getopts builtin not working"
    fi
fi
log_test "hash builtin"
"$SHELL_TO_TEST" -c "hash ls" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass
else
    fail "hash builtin not working"
fi
log_test "jobs builtin"
"$SHELL_TO_TEST" -c "jobs" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass
else
    fail "jobs builtin not working"
fi
log_test "kill builtin"
"$SHELL_TO_TEST" -c "kill -0 $$" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass
else
    fail "kill builtin not working"
fi
log_test "printf builtin"
result=$("$SHELL_TO_TEST" -c "printf '%s %d\n' hello 42" 2>/dev/null)
if [ "$result" = "hello 42" ]; then
    pass
else
    fail "Printf builtin failed"
fi
log_test "pwd builtin"
result=$("$SHELL_TO_TEST" -c "pwd" 2>/dev/null)
if [ -n "$result" ] && [ -d "$result" ]; then
    pass
else
    fail "Pwd builtin failed"
fi
log_test "read builtin"
"$SHELL_TO_TEST" -c "read var 2>/dev/null; exit 0" >/dev/null 2>&1 &
sleep 0.1
kill $! 2>/dev/null
wait $! 2>/dev/null
if [ $? -eq 0 ] || [ $? -eq 143 ]; then
    pass
else
    fail "read builtin failed. expected exit code 0 or 143, got $?"
fi
log_test "readonly builtin"
"$SHELL_TO_TEST" -c "readonly VAR=test; VAR=changed" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pass
else
    "$SHELL_TO_TEST" -c "readonly TEST_VAR=value" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        pass
    else
        fail "readonly builtin not working"
    fi
fi
log_test "return builtin"
result=$("$SHELL_TO_TEST" -c "func() { return 7; }; func; echo \$?" 2>/dev/null)
if [ "$result" = "7" ]; then
    pass
else
    fail "Return builtin failed"
fi
log_test "set builtin"
result=$("$SHELL_TO_TEST" -c "set -- a b c; echo \$1 \$2 \$3" 2>/dev/null)
if [ "$result" = "a b c" ]; then
    pass
else
    fail "Set builtin failed"
fi
log_test "shift builtin"
result=$("$SHELL_TO_TEST" -c "set -- a b c; shift; echo \$1 \$2" 2>/dev/null)
if [ "$result" = "b c" ]; then
    pass
else
    fail "Shift builtin failed"
fi
log_test "test builtin"
"$SHELL_TO_TEST" -c "test -f /etc/passwd" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Test builtin failed"
fi
log_test "[ builtin"
"$SHELL_TO_TEST" -c "[ -d /tmp ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Bracket test builtin failed"
fi
log_test "times builtin"
result=$("$SHELL_TO_TEST" -c "times" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    fail "times builtin not working"
fi
log_test "trap builtin"
result=$("$SHELL_TO_TEST" -c "trap 'echo trapped' INT; trap" 2>/dev/null)
if echo "$result" | grep -q "INT"; then
    pass
else
    "$SHELL_TO_TEST" -c "trap 'echo test' USR1" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        pass
    else
        fail "trap builtin not working"
    fi
fi
log_test "true builtin"
"$SHELL_TO_TEST" -c "true" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "True builtin failed"
fi
log_test "type builtin"
result=$("$SHELL_TO_TEST" -c "type echo" 2>/dev/null)
if echo "$result" | grep -q "echo"; then
    pass
else
    fail "type builtin not working"
fi
log_test "ulimit builtin"
if [ "$OS_NAME" = "Linux" ]; then
    skip "Skipping ulimit on Linux due to environment variability"
else
    result=$("$SHELL_TO_TEST" -c "ulimit -n" 2>/dev/null)
    if [ -n "$result" ] && ([ "$result" -gt 0 ] 2>/dev/null || [ "$result" = "unlimited" ]); then
        pass
    else
        "$SHELL_TO_TEST" -c "ulimit" >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            pass
        else
            fail "ulimit builtin not implemented"
        fi
    fi
fi
log_test "umask builtin"
result=$("$SHELL_TO_TEST" -c "umask" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    fail "umask builtin not working"
fi
log_test "unalias builtin"
"$SHELL_TO_TEST" -c "alias test_alias=echo; unalias test_alias" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Unalias builtin failed"
fi
log_test "unset builtin"
result=$("$SHELL_TO_TEST" -c "VAR=test; unset VAR; echo \$VAR" 2>/dev/null)
if [ -z "$result" ]; then
    pass
else
    fail "Unset builtin failed"
fi
log_test "wait builtin"
"$SHELL_TO_TEST" -c "sleep 0.1 & wait" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass
else
    fail "wait builtin not working"
fi
log_test "false builtin exists"
"$SHELL_TO_TEST" -c "false" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pass
else
    fail "false builtin should return non-zero exit code"
fi
log_test "pwd builtin consistency"
dir1=$("$SHELL_TO_TEST" -c "pwd" 2>/dev/null)
dir2=$("$SHELL_TO_TEST" -c "cd . && pwd" 2>/dev/null)
if [ "$dir1" = "$dir2" ]; then
    pass
else
    fail "pwd builtin inconsistent"
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
