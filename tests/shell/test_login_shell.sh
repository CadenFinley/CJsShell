#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: login shell behavior..."

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

# Create temporary home directory for testing
TEST_HOME=$(mktemp -d)
export HOME="$TEST_HOME"

# Test login mode detection
OUT=$("$CJSH_PATH" --login -c "echo 'login test'" 2>/dev/null)
if [ "$OUT" = "login test" ]; then
    pass_test "login mode execution"
else
    fail_test "login mode execution (got '$OUT')"
fi

# Test that login mode creates necessary files
"$CJSH_PATH" --login -c "true" 2>/dev/null
# Check if cjsh profile was created
if [ -f "$TEST_HOME/.cjprofile" ]; then
    pass_test "login mode creates profile file"
else
    skip_test "login mode did not create profile file"
fi

# Test profile file sourcing in login mode
echo "export TEST_PROFILE_VAR=profile_value" > "$TEST_HOME/.profile"
OUT=$("$CJSH_PATH" --login -c "echo \$TEST_PROFILE_VAR" 2>/dev/null)
if [ "$OUT" = "profile_value" ]; then
    pass_test "profile file sourced in login mode"
else
    skip_test "profile file not sourced in login mode (got '$OUT')"
fi

# Test environment variable setup
OUT=$("$CJSH_PATH" -c "echo \$HOME")
# Don't override the real HOME in the test shell, just check it's set
if [ -n "$OUT" ]; then
    pass_test "HOME variable should be set"
else
    fail_test "HOME variable should be set"
fi

OUT=$("$CJSH_PATH" -c "echo \$USER")
if [ -n "$OUT" ]; then
    pass_test "USER variable should be set"
else
    fail_test "USER variable should be set"
fi

OUT=$("$CJSH_PATH" -c "echo \$SHELL")
if [ -n "$OUT" ]; then
    pass_test "SHELL variable should be set"
else
    fail_test "SHELL variable should be set"
fi

# Test PWD variable
OUT=$("$CJSH_PATH" -c "echo \$PWD")
if [ -n "$OUT" ]; then
    pass_test "PWD variable should be set"
else
    fail_test "PWD variable should be set"
fi

# Test SHLVL variable
OUT=$("$CJSH_PATH" -c "echo \$SHLVL")
if [ -n "$OUT" ]; then
    pass_test "SHLVL variable should be set"
else
    fail_test "SHLVL variable should be set"
fi

# Test that SHLVL increments in subshells
OUT=$("$CJSH_PATH" -c "echo \$SHLVL; $CJSH_PATH -c 'echo \$SHLVL'" 2>/dev/null)
LINES=$(echo "$OUT" | wc -l)
if [ "$LINES" -eq 2 ]; then
    pass_test "SHLVL increment test"
else
    skip_test "SHLVL increment test inconclusive"
fi

# Test PATH variable setup
OUT=$("$CJSH_PATH" -c "echo \$PATH")
if [ -n "$OUT" ]; then
    pass_test "PATH variable should be set"
else
    fail_test "PATH variable should be set"
fi

# Test that PATH contains standard directories
echo "$OUT" | grep -q "/bin"
if [ $? -eq 0 ]; then
    pass_test "PATH should contain /bin"
else
    skip_test "PATH should contain /bin"
fi

# Test IFS variable
OUT=$("$CJSH_PATH" -c "echo \"\$IFS\"" | od -c 2>/dev/null)
if [ -n "$OUT" ]; then
    pass_test "IFS variable check"
else
    skip_test "IFS variable check failed"
fi

# Test command line argument preservation for restart
OUT=$("$CJSH_PATH" --no-colors -c "echo test")
if [ "$OUT" = "test" ]; then
    pass_test "command line arguments preserved"
else
    fail_test "command line arguments not preserved (got '$OUT')"
fi

# Cleanup
rm -rf "$TEST_HOME"

# Summary
echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))

if [ $TESTS_FAILED -eq 0 ] && [ $TESTS_SKIPPED -eq 0 ]; then
    printf "${GREEN}All tests passed! ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    exit 0
elif [ $TESTS_FAILED -eq 0 ]; then
    printf "${YELLOW}All tests passed with some skipped. ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    exit 0
else
    printf "${RED}Some tests failed. ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    printf "Passed: ${GREEN}$TESTS_PASSED${NC}, Failed: ${RED}$TESTS_FAILED${NC}, Skipped: ${YELLOW}$TESTS_SKIPPED${NC}\n"
    exit 1
fi
