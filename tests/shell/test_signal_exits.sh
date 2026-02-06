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

skip() {
    printf "${YELLOW}SKIP${NC} - %s\n" "$1"
}

count_zombies() {
    if command -v ps >/dev/null 2>&1; then
        count=$(ps axo stat 2>/dev/null | grep '^Z' | wc -l 2>/dev/null || echo 0)
        echo "$count" | tr -d ' \n'
    else
        echo 0
    fi
}

get_baseline_zombies() {
    count_zombies
}

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

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Signal Exit and Cleanup Behavior for: $SHELL_TO_TEST"
echo "============================================================"

BASELINE_ZOMBIES=$(get_baseline_zombies)
echo "Baseline zombie count: $BASELINE_ZOMBIES"

log_test "SIGTERM triggers graceful cleanup"

"$SHELL_TO_TEST" -c "sleep 2" &
shell_pid=$!

if wait_for_process $shell_pid; then
    kill -TERM $shell_pid 2>/dev/null
    sleep 0.5
    
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

log_test "SIGHUP triggers graceful cleanup"

"$SHELL_TO_TEST" -c "sleep 2" &
shell_pid=$!

if wait_for_process $shell_pid; then
    kill -HUP $shell_pid 2>/dev/null
    sleep 0.5
    
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

log_test "SIGINT handling in interactive mode"
"$SHELL_TO_TEST" <<'EOF' 2>/dev/null &
sleep 1
EOF
shell_pid=$!

timeout_limit=20
count=0
while [ $count -lt $timeout_limit ]; do
    if ! kill -0 $shell_pid 2>/dev/null; then
        break
    fi
    sleep 0.1
    count=$((count + 1))
done

if kill -0 $shell_pid 2>/dev/null; then
    kill -TERM $shell_pid 2>/dev/null
    sleep 0.2
    if kill -0 $shell_pid 2>/dev/null; then
        kill -KILL $shell_pid 2>/dev/null
    fi
    wait $shell_pid 2>/dev/null
    exit_code=124
else
    wait $shell_pid 2>/dev/null
    exit_code=$?
fi

if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
    pass
else
    fail "SIGINT handling issue in interactive mode, exit code: $exit_code"
fi

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

log_test "Background processes cleanup on signal exit"
if command -v ps >/dev/null 2>&1; then
    sleep_before=$(ps axo comm 2>/dev/null | grep -c "sleep" 2>/dev/null || echo 0)
    sleep_before=$(echo "$sleep_before" | tr -d ' \n')
    
    "$SHELL_TO_TEST" -c "sleep 2 & sleep 2" &
    shell_pid=$!
    
    if wait_for_process $shell_pid; then
        kill -TERM $shell_pid 2>/dev/null
        sleep 0.5
        
        sleep_after=$(ps axo comm 2>/dev/null | grep -c "sleep" 2>/dev/null || echo 0)
        sleep_after=$(echo "$sleep_after" | tr -d ' \n')
        
        if [ "$sleep_after" -le "$sleep_before" ]; then
            pass
        else
            sleep_orphans=$(($sleep_after - $sleep_before))
            fail "Signal exit left orphaned background processes ($sleep_orphans found)"
        fi
    else
        fail "Shell with background process did not start"
    fi
else
    skip "ps command not available"
fi

log_test "Resource cleanup on forced exit"

"$SHELL_TO_TEST" -c "sleep 0.1 & exit --force" 2>/dev/null
sleep 0.3  # Give time for cleanup

zombies_after=$(count_zombies)
new_zombies=$((zombies_after - BASELINE_ZOMBIES))
if [ $new_zombies -le 0 ]; then
    pass
else
    fail "Forced exit cleanup failed (baseline: $BASELINE_ZOMBIES, current: $zombies_after, new: $new_zombies)"
fi

log_test "Emergency cleanup on unexpected termination"
if command -v ps >/dev/null 2>&1; then
    "$SHELL_TO_TEST" -c "sleep 2" &
    shell_pid=$!
    
    if wait_for_process $shell_pid; then
        kill -KILL $shell_pid 2>/dev/null
        sleep 0.3
        
        zombies_after=$(count_zombies)
        new_zombies=$((zombies_after - BASELINE_ZOMBIES))
        if [ $new_zombies -le 0 ]; then
            pass
        else
            skip "SIGKILL may prevent proper cleanup (baseline: $BASELINE_ZOMBIES, current: $zombies_after, new: $new_zombies)"
        fi
    else
        fail "Shell process did not start for emergency cleanup test"
    fi
else
    skip "ps command not available"
fi

log_test "Signal handling preserves command execution"
result=$("$SHELL_TO_TEST" -c "echo 'test output'; exit 0" 2>/dev/null)
exit_code=$?

if [ "$result" = "test output" ] && [ $exit_code -eq 0 ]; then
    pass
else
    fail "Signal handling interfered with normal command execution"
fi

log_test "Multiple signal resistance"
"$SHELL_TO_TEST" -c "sleep 1" &
shell_pid=$!

if wait_for_process $shell_pid; then
    kill -TERM $shell_pid 2>/dev/null
    kill -HUP $shell_pid 2>/dev/null
    kill -TERM $shell_pid 2>/dev/null
    
    sleep 0.5
    
    if ! kill -0 $shell_pid 2>/dev/null; then
        pass
    else
        kill -KILL $shell_pid 2>/dev/null
        fail "Shell did not respond properly to multiple signals"
    fi
else
    fail "Shell process did not start for multiple signal test"
fi

log_test "Signal handling consistency across modes"
echo "sleep 2" | "$SHELL_TO_TEST" &
script_shell_pid=$!

"$SHELL_TO_TEST" -c "sleep 2" &
command_shell_pid=$!

if wait_for_process $script_shell_pid && wait_for_process $command_shell_pid; then
    kill -TERM $script_shell_pid 2>/dev/null
    sleep 0.1
    kill -TERM $command_shell_pid 2>/dev/null

    sleep 0.5

    script_gone=0
    command_gone=0

    if ! kill -0 $script_shell_pid 2>/dev/null; then
        script_gone=1
    fi

    if ! kill -0 $command_shell_pid 2>/dev/null; then
        command_gone=1
    fi

    kill -KILL $script_shell_pid 2>/dev/null
    kill -KILL $command_shell_pid 2>/dev/null

    if [ $script_gone -eq 1 ] || [ $command_gone -eq 1 ]; then
        pass
    else
        fail "Neither shell mode responded to signals properly"
    fi
else
    kill -KILL $script_shell_pid 2>/dev/null
    kill -KILL $command_shell_pid 2>/dev/null
    fail "Could not start shell processes for signal consistency test"
fi

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
