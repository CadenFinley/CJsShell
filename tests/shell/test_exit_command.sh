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

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Exit Command and Signal Exit Behavior for: $SHELL_TO_TEST"
echo "================================================================="

log_test "Basic exit with default code"
"$SHELL_TO_TEST" -c "exit" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Basic exit should return code 0, got $?"
fi

log_test "Exit with specific code (42)"
"$SHELL_TO_TEST" -c "exit 42" 2>/dev/null
if [ $? -eq 42 ]; then
    pass
else
    fail "Exit with code 42 should return 42, got $?"
fi

log_test "Exit with large code normalization"
"$SHELL_TO_TEST" -c "exit 300" 2>/dev/null
exit_code=$?
expected=44  # 300 & 255 = 44
if [ $exit_code -eq $expected ]; then
    pass
else
    fail "Exit with code 300 should return $expected, got $exit_code"
fi

log_test "Exit with negative code normalization"
"$SHELL_TO_TEST" -c "exit -1" 2>/dev/null
exit_code=$?
expected=255  # -1 & 255 = 255
if [ $exit_code -eq 255 ]; then
    pass
else
    fail "Exit with code -1 should return 255, got $exit_code"
fi

log_test "Exit with forced cleanup (--force)"
"$SHELL_TO_TEST" -c "exit --force" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with --force should return 0, got $?"
fi

log_test "Exit with forced cleanup and code (--force 55)"
"$SHELL_TO_TEST" -c "exit 55 --force" 2>/dev/null
if [ $? -eq 55 ]; then
    pass
else
    fail "Exit with --force and code 55 should return 55, got $?"
fi

log_test "Exit with forced cleanup (-f)"
"$SHELL_TO_TEST" -c "exit -f" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with -f should return 0, got $?"
fi

log_test "Exit with -f and code in different order"
"$SHELL_TO_TEST" -c "exit -f 77" 2>/dev/null
if [ $? -eq 77 ]; then
    pass
else
    fail "Exit with -f and code 77 should return 77, got $?"
fi

log_test "Exit with invalid argument"
"$SHELL_TO_TEST" -c "exit abc" 2>/dev/null
if [ $? -eq 128 ]; then
    pass
else
    fail "Exit with invalid argument should return 128, got $?"
fi

log_test "Exit within script content"
echo "echo 'Before exit'; exit 123; echo 'After exit'" | "$SHELL_TO_TEST" 2>/dev/null
if [ $? -eq 123 ]; then
    pass
else
    fail "Exit within script should return 123, got $?"
fi

log_test "SIGTERM signal handling (graceful exit)"
"$SHELL_TO_TEST" -c "sleep 2" &
shell_pid=$!
sleep 0.1  # Give shell time to start
kill -TERM $shell_pid 2>/dev/null
wait $shell_pid 2>/dev/null
exit_code=$?
if [ $exit_code -eq 143 ] || [ $exit_code -eq 0 ]; then
    pass
else
    fail "SIGTERM should cause graceful exit, got $exit_code"
fi

log_test "SIGHUP signal handling (graceful exit)"
"$SHELL_TO_TEST" -c "sleep 2" &
shell_pid=$!
sleep 0.1  # Give shell time to start
kill -HUP $shell_pid 2>/dev/null
wait $shell_pid 2>/dev/null
exit_code=$?
if [ $exit_code -eq 129 ] || [ $exit_code -eq 0 ]; then
    pass
else
    fail "SIGHUP should cause graceful exit, got $exit_code"
fi

log_test "EOF handling (Ctrl+D simulation)"
echo "" | "$SHELL_TO_TEST" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "EOF should cause graceful exit with code 0, got $?"
fi

log_test "Exit code propagation in -c mode"
"$SHELL_TO_TEST" -c "echo 'test'; exit 88" 2>/dev/null
if [ $? -eq 88 ]; then
    pass
else
    fail "Exit code should propagate in -c mode, expected 88, got $?"
fi

log_test "Multiple exit commands behavior"
"$SHELL_TO_TEST" -c "exit 11; exit 22" 2>/dev/null
if [ $? -eq 22 ]; then
    pass
else
    fail "Last exit should win, expected 22, got $?"
fi

log_test "Exit in conditional context"
"$SHELL_TO_TEST" -c "if true; then exit 99; fi" 2>/dev/null
if [ $? -eq 99 ]; then
    pass
else
    fail "Exit in conditional should work, expected 99, got $?"
fi

log_test "Exit with background process handling"
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

log_test "Exit builtin in background job"
exit_output=$("$SHELL_TO_TEST" -c "exit & jobs" 2>&1)
exit_status=$?
if [ $exit_status -eq 0 ] && ! printf '%s' "$exit_output" | grep -q "command not found" &&
   ! printf '%s' "$exit_output" | grep -q "Exit 127"; then
    pass
else
    fail "Background exit should not error (status=$exit_status, output=$exit_output)"
fi

log_test "Exit blocked when stopped jobs exist"
stopped_pid_file=$(mktemp /tmp/cjsh_stopped_exit.XXXXXX)
exit_output=$(PID_FILE="$stopped_pid_file" "$SHELL_TO_TEST" <<'EOF' 2>&1
sleep 1000 &
job=$!
echo "$job" > "$PID_FILE"
kill -STOP "$job"
sleep 0.2
exit 17
echo "exit-aborted"
kill -KILL "$job" 2>/dev/null
wait "$job" 2>/dev/null
exit 5
EOF
)
exit_status=$?
stopped_pid=$(cat "$stopped_pid_file" 2>/dev/null)
rm -f "$stopped_pid_file"
if [ -n "$stopped_pid" ] && kill -0 "$stopped_pid" 2>/dev/null; then
    kill -KILL "$stopped_pid" 2>/dev/null || true
fi
if [ $exit_status -eq 5 ] && printf '%s' "$exit_output" | grep -q "exit-aborted"; then
    pass
else
    fail "Exit should be blocked until stopped jobs clear (status=$exit_status, output=$exit_output)"
fi

log_test "Exit blocked when running jobs exist"
running_pid_file=$(mktemp /tmp/cjsh_running_exit.XXXXXX)
running_output=$(PID_FILE="$running_pid_file" "$SHELL_TO_TEST" <<'EOF' 2>&1
sleep 1000 &
job=$!
echo "$job" > "$PID_FILE"
exit 21
echo "exit-running-aborted"
kill -KILL "$job" 2>/dev/null
wait "$job" 2>/dev/null
exit 7
EOF
)
running_status=$?
running_pid=$(cat "$running_pid_file" 2>/dev/null)
rm -f "$running_pid_file"
if [ -n "$running_pid" ] && kill -0 "$running_pid" 2>/dev/null; then
    kill -KILL "$running_pid" 2>/dev/null || true
fi
if [ $running_status -eq 7 ] && printf '%s' "$running_output" | grep -q "exit-running-aborted"; then
    pass
else
    fail "Exit should be blocked until running jobs clear (status=$running_status, output=$running_output)"
fi

log_test "Forced exit bypasses stopped job guard"
force_pid_file=$(mktemp /tmp/cjsh_force_exit.XXXXXX)
PID_FILE="$force_pid_file" "$SHELL_TO_TEST" <<'EOF' >/dev/null 2>&1
sleep 1000 &
job=$!
echo "$job" > "$PID_FILE"
kill -STOP "$job"
sleep 0.2
exit --force 19
EOF
force_status=$?
force_pid=$(cat "$force_pid_file" 2>/dev/null)
rm -f "$force_pid_file"
if [ -n "$force_pid" ] && kill -0 "$force_pid" 2>/dev/null; then
    kill -KILL "$force_pid" 2>/dev/null || true
fi
if [ $force_status -eq 19 ]; then
    pass
else
    fail "Forced exit should succeed even with stopped jobs, got $force_status"
fi

log_test "Resource cleanup verification"
if command -v ps >/dev/null 2>&1; then
    zombies_before=$(ps axo stat 2>/dev/null | grep '^Z' | wc -l 2>/dev/null || echo 0)
    zombies_before=$(echo "$zombies_before" | tr -d ' \n')
    
    "$SHELL_TO_TEST" -c "sleep 0.1 & exit --force" 2>/dev/null
    
    sleep 0.2
    
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

log_test "Exit with mixed valid/invalid arguments"
"$SHELL_TO_TEST" -c "exit 50 invalid 60" 2>/dev/null
if [ $? -eq 50 ]; then
    pass
else
    fail "Exit should use first valid numeric argument, expected 50, got $?"
fi

log_test "Exit with only flags and no code"
"$SHELL_TO_TEST" -c "exit --force -f" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Exit with only flags should default to 0, got $?"
fi

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
