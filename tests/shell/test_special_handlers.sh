#!/usr/bin/env sh
# test_special_handlers.sh
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


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

OUT=$("$CJSH_PATH" -c '
function command_not_found_handler() {
    echo "handler-ran:$1"
    return 127
}

missing_command_with_default
echo "status:$?"
' 2>&1)

if echo "$OUT" | grep -q "handler-ran:missing_command_with_default" &&
   echo "$OUT" | grep -q "missing_command_with_default" &&
   echo "$OUT" | grep -q "command not found" &&
   echo "$OUT" | grep -q "status:127"; then
    pass_test "command_not_found_handler can fall back to default output"
else
    fail_test "command_not_found_handler default fallback failed (output: $OUT)"
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

for MODE_FLAG in --minimal --secure --posix; do
if OUT=$("$CJSH_PATH" "$MODE_FLAG" -c '
command_not_found_handler() {
    echo handler-should-not-run
    return 42
}

missing_mode_probe
echo "status:$?"
' 2>&1); then
    MODE_STATUS=0
else
    MODE_STATUS=$?
fi

    if [ "$MODE_STATUS" -eq 0 ] &&
       ! echo "$OUT" | grep -q "handler-should-not-run" &&
       echo "$OUT" | grep -q "command not found" &&
       echo "$OUT" | grep -q "status:127"; then
        pass_test "command_not_found_handler ignored in $MODE_FLAG"
    else
        fail_test "command_not_found_handler unexpectedly active in $MODE_FLAG (output: $OUT)"
    fi
done

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

for MODE_FLAG in --minimal --secure --posix; do
if OUT=$("$CJSH_PATH" "$MODE_FLAG" -c '
cjshexit() {
    echo cjshexit-should-not-run
}

true
' 2>&1); then
    MODE_STATUS=0
else
    MODE_STATUS=$?
fi

    if [ "$MODE_STATUS" -eq 0 ] &&
       ! echo "$OUT" | grep -q "cjshexit-should-not-run"; then
        pass_test "cjshexit ignored in $MODE_FLAG"
    else
        fail_test "cjshexit unexpectedly active in $MODE_FLAG (output: $OUT)"
    fi
done

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
