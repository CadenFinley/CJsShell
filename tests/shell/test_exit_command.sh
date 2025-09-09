#!/usr/bin/env sh
# Exit Command and Signal Exit Behavior Test Suite
# Tests exit command variations, signal handling, and resource cleanup

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

echo "Testing Exit Command and Signal Exit Behavior for: $SHELL_TO_TEST"
echo "================================================================="

# Test 1: Basic exit with default code (0)
log_test "Basic exit with default code"
"$SHELL_TO_TEST" -c "exit" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Basic exit should return code 0, got $?"
fi

# Test 2: Exit with specific code
log_test "Exit with specific code (42)"
"$SHELL_TO_TEST" -c "exit 42" 2>/dev/null
if [ $? -eq 42 ]; then
    pass
else
    fail "Exit with code 42 should return 42, got $?"
fi

# Test 3: Exit with large code (should be normalized to 0-255)
log_test "Exit with large code normalization"
"$SHELL_TO_TEST" -c "exit 300" 2>/dev/null
exit_code=$?
expected=44  # 300 & 255 = 44
if [ $exit_code -eq $expected ]; then
    pass
else
    fail "Exit with code 300 should return $expected, got $exit_code"
fi

# Test 4: Exit with negative code (should be normalized)
log_test "Exit with negative code normalization"
"$SHELL_TO_TEST" -c "exit -1" 2>/dev/null
exit_code=$?
expected=255  # -1 & 255 = 255
if [ $exit_code -eq 255 ]; then
    pass
else
    fail "Exit with code -1 should return 255, got $exit_code"
fi

# Test 5: Exit with forced cleanup flag
log_test "Exit with forced cleanup (--force)"
"$SHELL_TO_TEST" -c "exit --force" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with --force should return 0, got $?"
fi

# Test 6: Exit with forced cleanup and specific code
log_test "Exit with forced cleanup and code (--force 55)"
"$SHELL_TO_TEST" -c "exit 55 --force" 2>/dev/null
if [ $? -eq 55 ]; then
    pass
else
    fail "Exit with --force and code 55 should return 55, got $?"
fi

# Test 7: Exit with -f flag
log_test "Exit with forced cleanup (-f)"
"$SHELL_TO_TEST" -c "exit -f" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with -f should return 0, got $?"
fi

# Test 8: Exit with -f flag and code in different order
log_test "Exit with -f and code in different order"
"$SHELL_TO_TEST" -c "exit -f 77" 2>/dev/null
if [ $? -eq 77 ]; then
    pass
else
    fail "Exit with -f and code 77 should return 77, got $?"
fi

# Test 9: Exit with invalid argument (non-numeric)
log_test "Exit with invalid argument"
"$SHELL_TO_TEST" -c "exit abc" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with invalid argument should ignore and return 0, got $?"
fi

# Test 10: Exit within script execution
log_test "Exit within script content"
echo "echo 'Before exit'; exit 123; echo 'After exit'" | "$SHELL_TO_TEST" 2>/dev/null
if [ $? -eq 123 ]; then
    pass
else
    fail "Exit within script should return 123, got $?"
fi

# Test 11: Signal handling - SIGTERM graceful exit
log_test "SIGTERM signal handling (graceful exit)"
if command -v timeout >/dev/null 2>&1 || command -v gtimeout >/dev/null 2>&1; then
    # Start shell in background and send SIGTERM
    "$SHELL_TO_TEST" -c "sleep 10" &
    shell_pid=$!
    sleep 0.1  # Give shell time to start
    kill -TERM $shell_pid 2>/dev/null
    wait $shell_pid 2>/dev/null
    exit_code=$?
    # Shell should exit gracefully with SIGTERM (143 = 128 + 15)
    if [ $exit_code -eq 143 ] || [ $exit_code -eq 0 ]; then
        pass
    else
        fail "SIGTERM should cause graceful exit, got $exit_code"
    fi
else
    skip "timeout command not available"
fi

# Test 12: Signal handling - SIGHUP graceful exit
log_test "SIGHUP signal handling (graceful exit)"
if command -v timeout >/dev/null 2>&1 || command -v gtimeout >/dev/null 2>&1; then
    # Start shell in background and send SIGHUP
    "$SHELL_TO_TEST" -c "sleep 10" &
    shell_pid=$!
    sleep 0.1  # Give shell time to start
    kill -HUP $shell_pid 2>/dev/null
    wait $shell_pid 2>/dev/null
    exit_code=$?
    # Shell should exit gracefully with SIGHUP (129 = 128 + 1)
    if [ $exit_code -eq 129 ] || [ $exit_code -eq 0 ]; then
        pass
    else
        fail "SIGHUP should cause graceful exit, got $exit_code"
    fi
else
    skip "timeout command not available"
fi

# Test 13: EOF handling (Ctrl+D simulation)
log_test "EOF handling (Ctrl+D simulation)"
# Send empty input to simulate EOF
echo "" | "$SHELL_TO_TEST" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "EOF should cause graceful exit with code 0, got $?"
fi

# Test 14: Exit code propagation in command execution mode
log_test "Exit code propagation in -c mode"
"$SHELL_TO_TEST" -c "echo 'test'; exit 88" 2>/dev/null
if [ $? -eq 88 ]; then
    pass
else
    fail "Exit code should propagate in -c mode, expected 88, got $?"
fi

# Test 15: Multiple exit commands (last one wins in our implementation)
log_test "Multiple exit commands behavior"
"$SHELL_TO_TEST" -c "exit 11; exit 22" 2>/dev/null
if [ $? -eq 22 ]; then
    pass
else
    fail "Last exit should win, expected 22, got $?"
fi

# Test 16: Exit in conditional context
log_test "Exit in conditional context"
"$SHELL_TO_TEST" -c "if true; then exit 99; fi" 2>/dev/null
if [ $? -eq 99 ]; then
    pass
else
    fail "Exit in conditional should work, expected 99, got $?"
fi

# Test 17: Exit with background process cleanup
log_test "Exit with background process handling"
# This test ensures the shell exits properly even with background processes
"$SHELL_TO_TEST" -c "sleep 1 & exit 33" 2>/dev/null &
shell_pid=$!
sleep 0.5  # Give time for shell to start and exit
wait $shell_pid 2>/dev/null
exit_code=$?
if [ $exit_code -eq 33 ]; then
    pass
else
    fail "Exit should handle background processes, expected 33, got $exit_code"
fi

# Test 18: Cleanup verification (check for no zombies after force exit)
log_test "Resource cleanup verification"
if command -v ps >/dev/null 2>&1; then
    # Count zombie processes before
    zombies_before=$(ps axo stat 2>/dev/null | grep '^Z' | wc -l 2>/dev/null || echo 0)
    zombies_before=$(echo "$zombies_before" | tr -d ' \n')
    
    # Run shell with forced exit that should cleanup properly
    "$SHELL_TO_TEST" -c "sleep 0.1 & exit --force" 2>/dev/null
    
    # Give time for cleanup
    sleep 0.2
    
    # Count zombie processes after
    zombies_after=$(ps axo stat 2>/dev/null | grep '^Z' | wc -l 2>/dev/null || echo 0)
    zombies_after=$(echo "$zombies_after" | tr -d ' \n')
    
    if [ "$zombies_after" -le "$zombies_before" ]; then
        pass
    else
        fail "Resource cleanup may have failed (zombies: before=$zombies_before, after=$zombies_after)"
    fi
else
    skip "ps command not available for cleanup verification"
fi

# Test 19: Exit argument edge cases
log_test "Exit with mixed valid/invalid arguments"
"$SHELL_TO_TEST" -c "exit 50 invalid 60" 2>/dev/null
if [ $? -eq 50 ]; then
    pass
else
    fail "Exit should use first valid numeric argument, expected 50, got $?"
fi

# Test 20: Exit with only flags (no code)
log_test "Exit with only flags and no code"
"$SHELL_TO_TEST" -c "exit --force -f" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with only flags should default to 0, got $?"
fi

# Summary
echo ""
echo "Exit Command and Signal Test Results:"
echo "====================================="
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All exit and signal tests passed!${NC}"
    exit 0
else
    echo "${RED}Some tests failed.${NC}"
    exit 1
fi
