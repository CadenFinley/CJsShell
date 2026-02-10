#!/usr/bin/env sh

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

count_zombies() {
    if command -v ps >/dev/null 2>&1; then
        ps axo stat 2>/dev/null | awk '$1 ~ /^Z/ { count++ } END { print count+0 }'
    else
        echo 0
    fi
}

get_baseline_zombies() {
    count_zombies
}

check_for_zombies() {
    local timeout=${1:-5}
    local baseline=${2:-0}
    local max_checks=$((timeout * 10))
    local count=0
    local current_zombies=0
    local new_zombies=0
    while [ $count -lt $max_checks ]; do
        current_zombies=$(count_zombies)
        new_zombies=$((current_zombies - baseline))
        if [ "$new_zombies" -le 0 ]; then
            return 0
        fi
        sleep 0.1
        count=$((count + 1))
    done
    echo "Found $new_zombies new zombie(s) (total: $current_zombies, baseline: $baseline)"
    echo "Current zombies:"
    ps axo stat,pid,ppid,command 2>/dev/null | awk '$1 ~ /^Z/ { print }'
    return 1
}

echo "Test: SIGCHLD handling and zombie prevention..."

BASELINE_ZOMBIES=$(get_baseline_zombies)
echo "Baseline zombie count: $BASELINE_ZOMBIES"

"$CJSH_PATH" -c 'sleep 0.1 &; sleep 0.2'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: basic SIGCHLD handling"
else
    echo "FAIL: basic SIGCHLD handling - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'sleep 0.1 & sleep 0.1 & sleep 0.1 & sleep 0.3'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: multiple simultaneous children"
else
    echo "FAIL: multiple simultaneous children - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'true & true & true & wait'
if check_for_zombies 5 "$BASELINE_ZOMBIES"; then
    echo "PASS: limited fork scenario"
else
    echo "FAIL: limited fork scenario - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'false & PID=$!; sleep 0.1; wait $PID 2>/dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: immediately dying background process"
else
    echo "FAIL: immediately dying background process - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'sleep 0.1 & wait'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: parent exits leaving children"
else
    echo "FAIL: parent exits leaving children - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'echo test | head -1 | cat > /dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: pipeline with dying middle process"
else
    echo "FAIL: pipeline with dying middle process - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'sleep 2 & PID=$!; sleep 0.1; kill -TERM $PID; wait $PID 2>/dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: background job with signal handler"
else
    echo "FAIL: background job with signal handler - zombies found"
    exit 1
fi

RESULT=$("$CJSH_PATH" -c 'echo result')
if [ "$RESULT" = "result" ] && check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: command substitution with background"
else
    echo "FAIL: command substitution with background - zombies found or wrong result"
    exit 1
fi

"$CJSH_PATH" -c '(sleep 0.1 &; wait) > /dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: subshell with background jobs"
else
    echo "FAIL: subshell with background jobs - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'sleep 0.1 & wait'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: process group leadership"
else
    echo "FAIL: process group leadership - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'true & true & true & true & true & wait'
if check_for_zombies 5 "$BASELINE_ZOMBIES"; then
    echo "PASS: rapid signal delivery"
else
    echo "FAIL: rapid signal delivery - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'echo test > /tmp/cjsh_sigchld_test.out & wait; rm -f /tmp/cjsh_sigchld_test.out'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: background with output redirection"
else
    echo "FAIL: background with output redirection - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'sleep 0.2 & JOB_PID=$!; wait $JOB_PID'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: job control integration"
else
    echo "FAIL: job control integration - zombies found"
    exit 1
fi

"$CJSH_PATH" -c 'nonexistent_command 2>/dev/null & sleep 0.1; wait 2>/dev/null'
if check_for_zombies 3 "$BASELINE_ZOMBIES"; then
    echo "PASS: error in background process"
else
    echo "FAIL: error in background process - zombies found"
    exit 1
fi

sleep 0.5  # Give any lingering processes time to be reaped
if check_for_zombies 5 "$BASELINE_ZOMBIES"; then
    echo "PASS: final zombie check - no new zombies remaining"
else
    echo "FAIL: final zombie check - new zombies still present"
    echo "Current zombies:"
    ps axo stat,pid,ppid,command 2>/dev/null | awk '$1 ~ /^Z/ { print }'
    exit 1
fi

echo "PASS: All SIGCHLD and zombie prevention tests completed"
exit 0
