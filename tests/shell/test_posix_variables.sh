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

echo "Testing POSIX Variable and Parameter Expansion for: $SHELL_TO_TEST"
echo "=================================================================="

log_test "Basic variable assignment"
result=$("$SHELL_TO_TEST" -c "VAR=hello; echo \$VAR" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected 'hello', got '$result'"
fi

log_test "Variable assignment without spaces"
result=$("$SHELL_TO_TEST" -c "VAR=test123; echo \$VAR" 2>/dev/null)
if [ "$result" = "test123" ]; then
    pass
else
    fail "Variable assignment failed"
fi

log_test "Empty variable assignment"
result=$("$SHELL_TO_TEST" -c "VAR=; echo \"[\$VAR]\"" 2>/dev/null)
if [ "$result" = "[]" ]; then
    pass
else
    fail "Empty variable assignment failed"
fi

log_test "Parameter expansion \${VAR}"
result=$("$SHELL_TO_TEST" -c "VAR=test; echo \${VAR}" 2>/dev/null)
if [ "$result" = "test" ]; then
    pass
else
    fail "Braced parameter expansion failed"
fi

log_test "Default value \${VAR:-default}"
result=$("$SHELL_TO_TEST" -c "echo \${UNDEFINED:-default}" 2>/dev/null)
if [ "$result" = "default" ]; then
    pass
else
    fail "Default value expansion failed"
fi

log_test "Assignment \${VAR:=default}"
result=$("$SHELL_TO_TEST" -c "echo \${NEWVAR:=assigned}; echo \$NEWVAR" 2>/dev/null)
expected="assigned
assigned"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Assignment expansion failed"
fi

log_test "Error expansion \${VAR:?message}"
"$SHELL_TO_TEST" -c "echo \${UNDEFINED:?undefined variable}" 2>/dev/null
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    fail "Error expansion should fail"
fi

log_test "Alternative value \${VAR:+value}"
result=$("$SHELL_TO_TEST" -c "VAR=set; echo \${VAR:+alternative}" 2>/dev/null)
if [ "$result" = "alternative" ]; then
    pass
else
    fail "Alternative value expansion failed"
fi

log_test "String length \${#VAR}"
result=$("$SHELL_TO_TEST" -c "VAR=hello; echo \${#VAR}" 2>/dev/null)
if [ "$result" = "5" ]; then
    pass
else
    fail "String length expansion failed"
fi

log_test "Remove prefix \${VAR#pattern}"
result=$("$SHELL_TO_TEST" -c "VAR=hello.world; echo \${VAR#*.}" 2>/dev/null)
if [ "$result" = "world" ]; then
    pass
else
    fail "Prefix removal failed, got '$result'"
fi

log_test "Remove prefix \${VAR##pattern}"
result=$("$SHELL_TO_TEST" -c "VAR=one.two.three; echo \${VAR##*.}" 2>/dev/null)
if [ "$result" = "three" ]; then
    pass
else
    fail "Longest prefix removal failed"
fi

log_test "Remove suffix \${VAR%pattern}"
result=$("$SHELL_TO_TEST" -c "VAR=hello.world; echo \${VAR%.*}" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Suffix removal failed"
fi

log_test "Remove suffix \${VAR%%pattern}"
result=$("$SHELL_TO_TEST" -c "VAR=one.two.three; echo \${VAR%%.*}" 2>/dev/null)
if [ "$result" = "one" ]; then
    pass
else
    fail "Longest suffix removal failed"
fi

log_test "Positional parameters \$1, \$2, ..."
result=$("$SHELL_TO_TEST" -c "set -- first second third; echo \$1 \$2 \$3" 2>/dev/null)
if [ "$result" = "first second third" ]; then
    pass
else
    fail "Positional parameters failed"
fi

log_test "Parameter count \$#"
result=$("$SHELL_TO_TEST" -c "set -- a b c d; echo \$#" 2>/dev/null)
if [ "$result" = "4" ]; then
    pass
else
    fail "Parameter count failed"
fi

log_test "All parameters \$*"
result=$("$SHELL_TO_TEST" -c "set -- a b c; echo \$*" 2>/dev/null)
if [ "$result" = "a b c" ]; then
    pass
else
    fail "All parameters failed"
fi

log_test "All parameters \$@"
result=$("$SHELL_TO_TEST" -c "set -- a b c; echo \$@" 2>/dev/null)
if [ "$result" = "a b c" ]; then
    pass
else
    fail "All parameters @ failed"
fi

log_test "Quoted \"\$*\""
result=$("$SHELL_TO_TEST" -c "set -- a 'b c' d; echo \"\$*\"" 2>/dev/null)
if [ "$result" = "a b c d" ]; then
    pass
else
    fail "Quoted * expansion failed"
fi

log_test "Quoted \"\$@\""
result=$("$SHELL_TO_TEST" -c "set -- a 'b c' d; for arg in \"\$@\"; do echo \"[\$arg]\"; done" 2>/dev/null)
expected="[a]
[b c]
[d]"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Quoted @ expansion failed"
fi

log_test "Exit status \$?"
result=$("$SHELL_TO_TEST" -c "true; echo \$?" 2>/dev/null)
if [ "$result" = "0" ]; then
    pass
else
    fail "Exit status failed"
fi

log_test "Process ID \$$"
result=$("$SHELL_TO_TEST" -c "echo \$$" 2>/dev/null)
if [ -n "$result" ] && [ "$result" -gt 0 ] 2>/dev/null; then
    pass
else
    fail "Process ID failed"
fi

log_test "Background PID \$!"
result=$("$SHELL_TO_TEST" -c "sleep 0.1 & echo \$!" 2>/dev/null)
if [ -n "$result" ] && [ "$result" -gt 0 ] 2>/dev/null; then
    pass
else
    fail "Background PID not implemented"
fi

log_test "Variable in arithmetic"
result=$("$SHELL_TO_TEST" -c "a=5; b=3; echo \$((a + b))" 2>/dev/null)
if [ "$result" = "8" ]; then
    pass
else
    fail "Variable arithmetic failed"
fi

log_test "IFS and word splitting"
result=$("$SHELL_TO_TEST" -c "IFS=:; set -- a:b:c; echo \$2" 2>/dev/null)
if [ "$result" = "b" ]; then
    pass
else
    fail "IFS word splitting failed"
fi

log_test "Environment variable inheritance"
TEST_ENV_VAR=inherited "$SHELL_TO_TEST" -c "echo \$TEST_ENV_VAR" > /tmp/env_test_$$
result=$(cat /tmp/env_test_$$)
if [ "$result" = "inherited" ]; then
    pass
else
    fail "Environment inheritance failed"
fi
rm -f /tmp/env_test_$$

log_test "Variable scope in subshell"
result=$("$SHELL_TO_TEST" -c "VAR=outer; (VAR=inner; echo \$VAR); echo \$VAR" 2>/dev/null)
expected="inner
outer"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Subshell variable scope failed"
fi

log_test "Readonly variables"
"$SHELL_TO_TEST" -c "readonly RO_VAR=test; RO_VAR=changed" 2>/dev/null
if [ $? -ne 0 ]; then
    pass
else
    fail "Readonly not enforced"
fi

log_test "Unsetting variables"
result=$("$SHELL_TO_TEST" -c "VAR=test; unset VAR; echo \${VAR:-unset}" 2>/dev/null)
if [ "$result" = "unset" ]; then
    pass
else
    fail "Unset variable failed"
fi

log_test "Command substitution in variable"
result=$("$SHELL_TO_TEST" -c "VAR=\$(echo hello); echo \$VAR" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Command substitution in variable failed"
fi

log_test "Variable concatenation"
result=$("$SHELL_TO_TEST" -c "A=hello; B=world; echo \$A\$B" 2>/dev/null)
if [ "$result" = "helloworld" ]; then
    pass
else
    fail "Variable concatenation failed"
fi

echo "=================================================================="
echo "POSIX Variable and Parameter Expansion Test Results:"
echo "Total tests: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All variable tests passed!${NC}"
    exit 0
else
    echo "${YELLOW}Some variable tests failed. Review the failures above.${NC}"
    echo "Success rate: $(( PASSED * 100 / TOTAL ))%"
    exit 1
fi
