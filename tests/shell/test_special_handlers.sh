#!/usr/bin/env sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: special lifecycle handlers..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

OUT=$("$CJSH_PATH" -c '
function command_not_found_handler() {
    echo "handled:$1:$2:$3"
    return 42
}

missing_command_for_handler one two
echo "status:$?"
' 2>&1)

if echo "$OUT" | grep -q "handled:missing_command_for_handler:one:two" &&
   echo "$OUT" | grep -q "status:42" &&
   ! echo "$OUT" | grep -q "command not found"; then
    pass_test "command_not_found_handler overrides message and exit status"
else
    fail_test "command_not_found_handler did not behave as expected (output: $OUT)"
fi

OUT=$("$CJSH_PATH" -c "missing_command_without_handler" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 127 ] &&
   echo "$OUT" | grep -q "missing_command_without_handler" &&
   echo "$OUT" | grep -q "command not found"; then
    pass_test "default command-not-found behavior remains without handler"
else
    fail_test "default command-not-found behavior changed unexpectedly (status=$STATUS output: $OUT)"
fi

OUT=$("$CJSH_PATH" -c '
echo "shell_pid:$$"

function command_not_found_handler() {
    echo "handler_pid:$$"
    return 13
}

missing_handler_pid_check
echo "status:$?"
' 2>&1)

SHELL_PID=$(printf "%s\n" "$OUT" | grep '^shell_pid:' | cut -d: -f2)
HANDLER_PID=$(printf "%s\n" "$OUT" | grep '^handler_pid:' | cut -d: -f2)

if [ -n "$SHELL_PID" ] && [ -n "$HANDLER_PID" ] && [ "$SHELL_PID" = "$HANDLER_PID" ] &&
   echo "$OUT" | grep -q "status:13"; then
    pass_test "command_not_found_handler runs in active shell process"
else
    fail_test "command_not_found_handler process context changed unexpectedly (output: $OUT)"
fi

OUT=$("$CJSH_PATH" -c '
function command_not_found_handler() {
    (sleep 0.01) &
    return 127
}

missing_background_probe
' 2>&1)

if ! echo "$OUT" | grep -q "\[[0-9][0-9]*\]"; then
    pass_test "command_not_found_handler background work does not emit job notification"
else
    fail_test "unexpected job notification from command_not_found_handler background work (output: $OUT)"
fi

OUT=$("$CJSH_PATH" -c '
function command_not_found_handler() {
    missing_inside_handler
    return 88
}

outer_missing_command
echo "status:$?"
' 2>&1)

if echo "$OUT" | grep -q "status:88"; then
    pass_test "command_not_found_handler recursion guard allows handler completion"
else
    fail_test "command_not_found_handler recursion handling failed (output: $OUT)"
fi

OUT=$("$CJSH_PATH" -c '
function cjshexit() {
    echo cjshexit-ran
}

true
' 2>&1)

if echo "$OUT" | grep -q "cjshexit-ran"; then
    pass_test "cjshexit runs during shell shutdown"
else
    fail_test "cjshexit did not run (output: $OUT)"
fi

OUT=$("$CJSH_PATH" -c '
function cjshexit() {
    echo cjshexit-first
}

trap "echo trap-exit-second" EXIT
true
' 2>&1)

ORDER=$(printf "%s\n" "$OUT" | sed '/^$/d')
if [ "$ORDER" = "cjshexit-first
trap-exit-second" ]; then
    pass_test "cjshexit runs before EXIT trap"
else
    fail_test "unexpected cjshexit/EXIT trap order (output: $OUT)"
fi

echo ""
echo "Special Handler Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ "$TESTS_FAILED" -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
