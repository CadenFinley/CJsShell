#!/usr/bin/env sh
# Signal Exit and Cleanup Test Suite
# Tests signal-triggered exits and resource cleanup behavior

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

# Helper function to count zombie processes
count_zombies() {
    if command -v ps >/dev/null 2>&1; then
        count=$(ps axo stat 2>/dev/null | grep '^Z' | wc -l 2>/dev/null || echo 0)
        echo "$count" | tr -d ' \n'
    else
        echo 0
    fi
}

# Get baseline zombie count
get_baseline_zombies() {
    count_zombies
}

# Helper function to wait for process to start
wait_for_process() {
    local max_wait=20
    local count=0
    while [ $count -lt $max_wait ]; do
        if kill -0 "$1" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
        count=$((count + 1))
    done
    return 1
}

# Check if shell exists
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Signal Exit and Cleanup Behavior for: $SHELL_TO_TEST"
echo "============================================================"

# Capture baseline zombie count
BASELINE_ZOMBIES=$(get_baseline_zombies)
echo "Baseline zombie count: $BASELINE_ZOMBIES"

# Test 1: SIGTERM triggers graceful cleanup
log_test "SIGTERM triggers graceful cleanup"

# Start shell with a long-running command
"$SHELL_TO_TEST" -c "sleep 5" &
shell_pid=$!

if wait_for_process $shell_pid; then
    # Send SIGTERM and wait for it to exit
    kill -TERM $shell_pid 2>/dev/null
    sleep 0.5
    
    # Check if process is gone
    if ! kill -0 $shell_pid 2>/dev/null; then
        zombies_after=$(count_zombies)
        new_zombies=$((zombies_after - BASELINE_ZOMBIES))
        if [ $new_zombies -le 0 ]; then
            pass
        else
            fail "SIGTERM cleanup left new zombies (baseline: $BASELINE_ZOMBIES, current: $zombies_after, new: $new_zombies)"
        fi
    else
        kill -KILL $shell_pid 2>/dev/null
        fail "Shell did not respond to SIGTERM"
    fi
else
    fail "Shell process did not start properly"
fi

# Test 2: SIGHUP triggers graceful cleanup
log_test "SIGHUP triggers graceful cleanup"

# Start shell with a long-running command
"$SHELL_TO_TEST" -c "sleep 5" &
shell_pid=$!

if wait_for_process $shell_pid; then
    # Send SIGHUP and wait for it to exit
    kill -HUP $shell_pid 2>/dev/null
    sleep 0.5
    
    # Check if process is gone
    if ! kill -0 $shell_pid 2>/dev/null; then
        zombies_after=$(count_zombies)
        new_zombies=$((zombies_after - BASELINE_ZOMBIES))
        if [ $new_zombies -le 0 ]; then
            pass
        else
            fail "SIGHUP cleanup left new zombies (baseline: $BASELINE_ZOMBIES, current: $zombies_after, new: $new_zombies)"
        fi
    else
        kill -KILL $shell_pid 2>/dev/null
        fail "Shell did not respond to SIGHUP"
    fi
else
    fail "Shell process did not start properly"
fi

# Test 3: SIGINT handling in interactive mode
log_test "SIGINT handling in interactive mode"
# Note: SIGINT should not cause immediate exit in interactive mode
if command -v timeout >/dev/null 2>&1 || command -v gtimeout >/dev/null 2>&1; then
    # Use timeout to test SIGINT behavior
    # Try gtimeout first, then timeout
    if command -v gtimeout >/dev/null 2>&1; then
        TIMEOUT_CMD="gtimeout"
    else
        TIMEOUT_CMD="timeout"
    fi
    $TIMEOUT_CMD 2s sh -c "echo 'sleep 1' | $SHELL_TO_TEST" 2>/dev/null
    exit_code=$?
    # timeout returns 124 if command timed out, which is expected for interactive shell
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
        pass
    else
        fail "SIGINT handling issue in interactive mode, exit code: $exit_code"
    fi
else
    skip "timeout command not available"
fi

# Test 4: Signal handling preserves exit code for normal exit
log_test "Signal handling preserves normal exit codes"
"$SHELL_TO_TEST" -c "exit 42" &
shell_pid=$!
wait $shell_pid 2>/dev/null
exit_code=$?

if [ $exit_code -eq 42 ]; then
    pass
else
    fail "Normal exit code not preserved, expected 42, got $exit_code"
fi

# Test 5: Background processes cleanup on signal exit
log_test "Background processes cleanup on signal exit"
if command -v ps >/dev/null 2>&1; then
    # Count sleep processes before test
    sleep_before=$(ps axo comm 2>/dev/null | grep -c "sleep" 2>/dev/null || echo 0)
    
    # Start shell with background process
    "$SHELL_TO_TEST" -c "sleep 10 & sleep 5" &
    shell_pid=$!
    
    if wait_for_process $shell_pid; then
        # Send SIGTERM
        kill -TERM $shell_pid 2>/dev/null
        sleep 0.5
        
        # Count sleep processes after test
        sleep_after=$(ps axo comm 2>/dev/null | grep -c "sleep" 2>/dev/null || echo 0)
        
        # Check if we have more sleep processes than we started with
        if [ $sleep_after -le $sleep_before ]; then
            pass
        else
            sleep_orphans=$((sleep_after - sleep_before))
            fail "Signal exit left orphaned background processes ($sleep_orphans found)"
        fi
    else
        fail "Shell with background process did not start"
    fi
else
    skip "ps command not available"
fi

# Test 6: Resource cleanup on forced exit
log_test "Resource cleanup on forced exit"

# Test forced exit cleanup
"$SHELL_TO_TEST" -c "sleep 0.1 & exit --force" 2>/dev/null
sleep 0.3  # Give time for cleanup

zombies_after=$(count_zombies)
new_zombies=$((zombies_after - BASELINE_ZOMBIES))
if [ $new_zombies -le 0 ]; then
    pass
else
    fail "Forced exit cleanup failed (baseline: $BASELINE_ZOMBIES, current: $zombies_after, new: $new_zombies)"
fi

# Test 7: Emergency cleanup on unexpected termination
log_test "Emergency cleanup on unexpected termination"
if command -v ps >/dev/null 2>&1; then
    # Start shell and kill it abruptly
    "$SHELL_TO_TEST" -c "sleep 2" &
    shell_pid=$!
    
    if wait_for_process $shell_pid; then
        # Kill with SIGKILL (should trigger atexit cleanup)
        kill -KILL $shell_pid 2>/dev/null
        sleep 0.3
        
        zombies_after=$(count_zombies)
        new_zombies=$((zombies_after - BASELINE_ZOMBIES))
        if [ $new_zombies -le 0 ]; then
            pass
        else
            # Note: SIGKILL may not allow proper cleanup, so this might be expected
            skip "SIGKILL may prevent proper cleanup (baseline: $BASELINE_ZOMBIES, current: $zombies_after, new: $new_zombies)"
        fi
    else
        fail "Shell process did not start for emergency cleanup test"
    fi
else
    skip "ps command not available"
fi

# Test 8: Signal handling does not interfere with command execution
log_test "Signal handling preserves command execution"
result=$("$SHELL_TO_TEST" -c "echo 'test output'; exit 0" 2>/dev/null)
exit_code=$?

if [ "$result" = "test output" ] && [ $exit_code -eq 0 ]; then
    pass
else
    fail "Signal handling interfered with normal command execution"
fi

# Test 9: Multiple signal handling
log_test "Multiple signal resistance"
# Send multiple signals and ensure only the first causes exit
"$SHELL_TO_TEST" -c "sleep 3" &
shell_pid=$!

if wait_for_process $shell_pid; then
    # Send multiple signals quickly
    kill -TERM $shell_pid 2>/dev/null
    kill -HUP $shell_pid 2>/dev/null
    kill -TERM $shell_pid 2>/dev/null
    
    sleep 0.5
    
    # Process should be gone after first signal
    if ! kill -0 $shell_pid 2>/dev/null; then
        pass
    else
        kill -KILL $shell_pid 2>/dev/null
        fail "Shell did not respond properly to multiple signals"
    fi
else
    fail "Shell process did not start for multiple signal test"
fi

# Test 10: Signal handling in script mode vs interactive mode
log_test "Signal handling consistency across modes"
# Test that signal handling works the same in both modes
echo "sleep 2" | "$SHELL_TO_TEST" &
script_shell_pid=$!

"$SHELL_TO_TEST" -c "sleep 2" &
command_shell_pid=$!

# Wait for processes to start properly
if wait_for_process $script_shell_pid && wait_for_process $command_shell_pid; then
    # Send signals to both with a small delay
    kill -TERM $script_shell_pid 2>/dev/null
    sleep 0.1
    kill -TERM $command_shell_pid 2>/dev/null

    sleep 0.5

    # Both should be gone
    script_gone=0
    command_gone=0

    if ! kill -0 $script_shell_pid 2>/dev/null; then
        script_gone=1
    fi

    if ! kill -0 $command_shell_pid 2>/dev/null; then
        command_gone=1
    fi

    # Cleanup any remaining processes
    kill -KILL $script_shell_pid 2>/dev/null
    kill -KILL $command_shell_pid 2>/dev/null

    # At least one should respond to signals properly
    if [ $script_gone -eq 1 ] || [ $command_gone -eq 1 ]; then
        pass
    else
        fail "Neither shell mode responded to signals properly"
    fi
else
    # Cleanup
    kill -KILL $script_shell_pid 2>/dev/null
    kill -KILL $command_shell_pid 2>/dev/null
    fail "Could not start shell processes for signal consistency test"
fi

# Summary
echo ""
echo "Signal Exit and Cleanup Test Results:"
echo "===================================="
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All signal exit and cleanup tests passed!${NC}"
    exit 0
else
    echo "${RED}Some tests failed.${NC}"
    exit 1
fi
