#!/usr/bin/env sh

TOTAL=0
PASSED=0
FAILED=0

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
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

if [ ! -x "$CJSH_PATH" ]; then
    echo "${RED}Error: Shell '$CJSH_PATH' not found or not executable${NC}"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Zombie Process Handling for: $CJSH_PATH"
echo "================================================"

count_zombies() {
    ps aux | awk '$8 ~ /^Z/ { count++ } END { print count+0 }'
}

is_zombie() {
    local pid=$1
    if [ -n "$pid" ] && [ "$pid" -gt 0 ]; then
        ps -o stat= -p "$pid" 2>/dev/null | grep -q "^Z"
    else
        return 1
    fi
}

log_test "No zombies from simple commands"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "echo hello > /dev/null"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Simple command created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Background process with wait doesn't create zombies"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "sleep 0.1 & wait"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Background process with wait created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Multiple background processes cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "sleep 0.1 & sleep 0.1 & sleep 0.1 & wait"
sleep 0.2  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Multiple background processes created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Pipeline processes don't create zombies"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "echo test | cat | wc -l > /dev/null"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Pipeline created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Killed background process cleanup"
ZOMBIES_BEFORE=$(count_zombies)
RESULT=$("$CJSH_PATH" -c "sleep 2 & echo \$!; kill %1; wait %1 2>/dev/null; echo done" 2>/dev/null)
sleep 0.2  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Killed background process created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Quick-exiting background processes cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "true & true & true & true & true &"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Quick-exiting processes created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Command substitution cleanup"
ZOMBIES_BEFORE=$(count_zombies)
RESULT=$("$CJSH_PATH" -c "echo \$(echo nested \$(echo double))")
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ] && [ "$RESULT" = "nested double" ]; then
    pass
else
    fail "Command substitution created zombies or failed (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER, result: '$RESULT')"
fi

log_test "Process group cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "{ sleep 0.1; echo group; } &"
sleep 0.2  # Give time for completion and cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Process group created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Rapid process creation/termination"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "for i in 1 2 3 4 5; do true & done; wait"
sleep 0.2  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Rapid process creation created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Mixed foreground/background process cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "echo fg1; sleep 0.1 & echo fg2; wait; echo fg3"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Mixed processes created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Subshell process cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "(echo subshell; sleep 0.1) &"
sleep 0.2  # Give time for completion and cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Subshell created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Complex pipeline cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "echo 'line1\nline2\nline3' | grep line | wc -l > /dev/null"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Complex pipeline created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Error in background process cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "nonexistent_command_xyz123 & wait %1 2>/dev/null"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Error in background process created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Signal handling doesn't interfere with cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "sleep 2 & PID=\$!; kill -TERM \$PID; wait \$PID 2>/dev/null"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Signal handling interfered with cleanup (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

log_test "Job control commands cleanup"
ZOMBIES_BEFORE=$(count_zombies)
"$CJSH_PATH" -c "sleep 0.1 & jobs > /dev/null; wait"
sleep 0.1  # Give time for cleanup
ZOMBIES_AFTER=$(count_zombies)
if [ "$ZOMBIES_AFTER" -le "$ZOMBIES_BEFORE" ]; then
    pass
else
    fail "Job control commands created zombies (before: $ZOMBIES_BEFORE, after: $ZOMBIES_AFTER)"
fi

echo ""
echo "================================================"
echo "Zombie Process Test Summary:"
echo "  Total tests: $TOTAL"
echo "  Passed:      ${GREEN}$PASSED${NC}"
echo "  Failed:      ${RED}$FAILED${NC}"
echo "================================================"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All zombie process tests passed!${NC}"
    exit 0
else
    echo "${RED}Some zombie process tests failed.${NC}"
    exit 1
fi
