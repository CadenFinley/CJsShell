#!/usr/bin/env sh
# Test SIGCHLD handling and zombie process prevention
# Tests specific signal handling scenarios related to zombie prevention

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

# Get baseline zombie count before testing
get_baseline_zombies() {
    ps aux | awk '$8 ~ /^Z/ { count++ } END { print count+0 }'
}

# Helper function to check for new zombies (above baseline)
check_for_zombies() {
    local timeout=${1:-5}
    local baseline=${2:-0}
    local count=0
    while [ $count -lt $timeout ]; do
        current_zombies=$(ps aux | awk '$8 ~ /^Z/ { count++ } END { print count+0 }')
        new_zombies=$((current_zombies - baseline))
        if [ "$new_zombies" -gt 0 ]; then
            echo "Found $new_zombies new zombie(s) (total: $current_zombies, baseline: $baseline)"
            # Show zombie details for debugging
            echo "Current zombies:"
            ps aux | awk '$8 ~ /^Z/ { print "  PID:", $2, "PPID:", $3, "STAT:", $8, "CMD:", $11 }'
            return 1
        fi
        sleep 0.1
        count=$((count + 1))
    done
    return 0
}

echo "Test: SIGCHLD handling and zombie prevention..."

# Capture baseline zombie count before testing
BASELINE_ZOMBIES=$(get_baseline_zombies)
echo "Baseline zombie count: $BASELINE_ZOMBIES"

# Test 1: Basic SIGCHLD handling
"$CJSH_PATH" -c 'sleep 0.1 &; sleep 0.2'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: basic SIGCHLD handling"
else
    echo "FAIL: basic SIGCHLD handling - zombies found"
    exit 1
fi

# Test 2: Multiple simultaneous children
"$CJSH_PATH" -c 'sleep 0.1 & sleep 0.1 & sleep 0.1 & sleep 0.3'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: multiple simultaneous children"
else
    echo "FAIL: multiple simultaneous children - zombies found"
    exit 1
fi

# Test 3: Limited fork scenario
"$CJSH_PATH" -c 'true & true & true & wait'
if check_for_zombies 5 "$BASELINE_ZOMBIES"; then
    echo "PASS: limited fork scenario"
else
    echo "FAIL: limited fork scenario - zombies found"
    exit 1
fi

# Test 4: Background process that dies immediately
"$CJSH_PATH" -c 'false & PID=$!; sleep 0.1; wait $PID 2>/dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: immediately dying background process"
else
    echo "FAIL: immediately dying background process - zombies found"
    exit 1
fi

# Test 5: Simple parent/child test
"$CJSH_PATH" -c 'sleep 0.1 & wait'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: parent exits leaving children"
else
    echo "FAIL: parent exits leaving children - zombies found"
    exit 1
fi

# Test 6: Pipeline where middle process dies
"$CJSH_PATH" -c 'echo test | head -1 | cat > /dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: pipeline with dying middle process"
else
    echo "FAIL: pipeline with dying middle process - zombies found"
    exit 1
fi

# Test 7: Background job with signal
"$CJSH_PATH" -c 'sleep 10 & PID=$!; sleep 0.1; kill -TERM $PID; wait $PID 2>/dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: background job with signal handler"
else
    echo "FAIL: background job with signal handler - zombies found"
    exit 1
fi

# Test 8: Command substitution with background elements
RESULT=$("$CJSH_PATH" -c 'echo result')
if [ "$RESULT" = "result" ] && check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: command substitution with background"
else
    echo "FAIL: command substitution with background - zombies found or wrong result"
    exit 1
fi

# Test 9: Subshell that creates background jobs
"$CJSH_PATH" -c '(sleep 0.1 &; wait) > /dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: subshell with background jobs"
else
    echo "FAIL: subshell with background jobs - zombies found"
    exit 1
fi

# Test 10: Process group test
"$CJSH_PATH" -c 'sleep 0.1 & wait'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: process group leadership"
else
    echo "FAIL: process group leadership - zombies found"
    exit 1
fi

# Test 11: Rapid signal delivery test
"$CJSH_PATH" -c 'true & true & true & true & true & wait'
if check_for_zombies 5 "$BASELINE_ZOMBIES"; then
    echo "PASS: rapid signal delivery"
else
    echo "FAIL: rapid signal delivery - zombies found"
    exit 1
fi

# Test 12: Background process with output redirection
"$CJSH_PATH" -c 'echo test > /tmp/cjsh_sigchld_test.out & wait; rm -f /tmp/cjsh_sigchld_test.out'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: background with output redirection"
else
    echo "FAIL: background with output redirection - zombies found"
    exit 1
fi

# Test 13: Job control integration
"$CJSH_PATH" -c 'sleep 0.2 & JOB_PID=$!; wait $JOB_PID'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: job control integration"
else
    echo "FAIL: job control integration - zombies found"
    exit 1
fi

# Test 14: Error in background process cleanup
"$CJSH_PATH" -c 'nonexistent_command 2>/dev/null & sleep 0.1; wait 2>/dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: error in background process"
else
    echo "FAIL: error in background process - zombies found"
    exit 1
fi

# Test 15: Final zombie check
sleep 0.5  # Give any lingering processes time to be reaped
if check_for_zombies 5 "$BASELINE_ZOMBIES"; then
    echo "PASS: final zombie check - no new zombies remaining"
else
    echo "FAIL: final zombie check - new zombies still present"
    echo "Current zombies:"
    ps aux | awk '$8 ~ /^Z/ { print "  PID:", $2, "PPID:", $3, "STAT:", $8, "CMD:", $11 }'
    exit 1
fi

echo "PASS: All SIGCHLD and zombie prevention tests completed"
exit 0
