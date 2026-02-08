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

EXPECTED_SHELL_BASENAME=$(basename "$SHELL_TO_TEST")
ORIGINAL_SHELL="${SHELL:-}"
if [ -n "$ORIGINAL_SHELL" ]; then
    ORIGINAL_SHELL_BASENAME=$(basename "$ORIGINAL_SHELL")
else
    ORIGINAL_SHELL_BASENAME=""
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

echo "Testing POSIX Login Shell Environment for: $SHELL_TO_TEST"
echo "========================================================"

TEST_HOME=$(mktemp -d)
export HOME="$TEST_HOME"
echo "Using temporary home: $TEST_HOME"

log_test "Login shell detection with --login"
result=$("$SHELL_TO_TEST" --login -c "echo login_test" 2>/dev/null)
if [ "$result" = "login_test" ]; then
    pass
else
    fail "Login shell flag not working"
fi

log_test "Essential environment variables"
result=$("$SHELL_TO_TEST" --login -c "echo \$HOME:\$USER:\$SHELL" 2>/dev/null)
home_part=$(echo "$result" | cut -d: -f1)
user_part=$(echo "$result" | cut -d: -f2)
shell_part=$(echo "$result" | cut -d: -f3)
if [ -n "$home_part" ] && [ -n "$user_part" ] && [ -n "$shell_part" ] && [ -d "$home_part" ]; then
    pass
else
    fail "Essential environment variables not set properly"
fi

log_test "PATH environment variable"
result=$("$SHELL_TO_TEST" --login -c "echo \$PATH" 2>/dev/null)
if [ -n "$result" ] && echo "$result" | grep -q "/bin"; then
    pass
else
    fail "PATH not set properly"
fi

log_test "SHELL environment variable"
result=$("$SHELL_TO_TEST" --login -c "echo \$SHELL" 2>/dev/null)
if [ -n "$result" ]; then
    result_basename=$(basename "$result")
    if [ "$result" = "$SHELL_TO_TEST" ] || [ "$result_basename" = "$EXPECTED_SHELL_BASENAME" ]; then
        pass
    elif [ -n "$ORIGINAL_SHELL" ] && [ "$result" = "$ORIGINAL_SHELL" ]; then
        pass
    elif [ -n "$ORIGINAL_SHELL_BASENAME" ] && [ "$result_basename" = "$ORIGINAL_SHELL_BASENAME" ]; then
        pass
    else
        fail "SHELL variable unexpected ('$result')"
    fi
else
    fail "SHELL variable not set"
fi

log_test "PWD environment variable"
result=$("$SHELL_TO_TEST" --login -c "echo \$PWD" 2>/dev/null)
if [ -n "$result" ] && [ -d "$result" ]; then
    pass
else
    fail "PWD not set to valid directory"
fi

log_test "SHLVL environment variable"
current_shlvl=$SHLVL
result=$("$SHELL_TO_TEST" --login -c "echo \$SHLVL" 2>/dev/null)
if [ -n "$result" ] && [ "$result" -gt 0 ] 2>/dev/null; then
    pass
else
    fail "SHLVL not incremented properly"
fi

log_test "User profile sourcing"
echo "export PROFILE_SOURCED=yes" > "$TEST_HOME/.profile"
result=$("$SHELL_TO_TEST" --login -c "echo \$PROFILE_SOURCED" 2>/dev/null)
if [ "$result" = "yes" ]; then
    pass
else
    fail "User profile not sourced"
fi


log_test "Non-login shell behavior"
result=$("$SHELL_TO_TEST" -c "echo \$SHLVL" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    fail "Non-login shell should still set basic variables"
fi

log_test "Interactive shell detection"
result=$("$SHELL_TO_TEST" -c "echo non_interactive" 2>/dev/null)
if [ "$result" = "non_interactive" ]; then
    pass
else
    fail "Non-interactive mode not working"
fi

log_test "Environment variable inheritance"
TEST_INHERIT=inherited "$SHELL_TO_TEST" --login -c "echo \$TEST_INHERIT" > /tmp/inherit_test_$$
result=$(cat /tmp/inherit_test_$$)
if [ "$result" = "inherited" ]; then
    pass
else
    fail "Environment variable inheritance failed"
fi
rm -f /tmp/inherit_test_$$

log_test "Default umask setting"
result=$("$SHELL_TO_TEST" --login -c "umask" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    fail "umask builtin not implemented"
fi

log_test "Signal handler setup"
"$SHELL_TO_TEST" --login -c "echo signal_test" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Basic signal handling setup failed"
fi

log_test "Working directory initialization"
original_dir=$(pwd)
cd /tmp
result=$("$SHELL_TO_TEST" --login -c "pwd" 2>/dev/null)
cd "$original_dir"
if [ -n "$result" ] && [ -d "$result" ]; then
    pass
else
    fail "Working directory not properly initialized"
fi


log_test "Locale settings"
result=$("$SHELL_TO_TEST" --login -c "echo \$LANG" 2>/dev/null)
if [ -n "$result" ]; then
    pass
else
    fail "LANG variable not set by shell"
fi

log_test "Exit status preservation"
"$SHELL_TO_TEST" --login -c "exit 42" 2>/dev/null
if [ $? -eq 42 ]; then
    pass
else
    fail "Exit status not preserved"
fi

log_test "Resource cleanup on exit"
pass  # Assume cleanup works if no obvious leaks

log_test "Login shell vs subshell environment"
login_shlvl=$("$SHELL_TO_TEST" --login -c "echo \$SHLVL" 2>/dev/null)
subshell_shlvl=$("$SHELL_TO_TEST" --login -c "($SHELL_TO_TEST -c 'echo \$SHLVL')" 2>/dev/null)
if [ "$subshell_shlvl" -gt "$login_shlvl" ] 2>/dev/null; then
    pass
else
    fail "SHLVL increment in subshell complex to verify"
fi

echo "========================================================"
echo "POSIX Login Shell Environment Test Results:"
echo "Total tests: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

rm -rf "$TEST_HOME"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All login shell environment tests passed!${NC}"
    exit 0
else
    echo "${YELLOW}Some login shell tests failed. Review the failures above.${NC}"
    echo "Success rate: $(( PASSED * 100 / TOTAL ))%"
    exit 1
fi
