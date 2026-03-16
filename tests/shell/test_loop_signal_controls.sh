#!/usr/bin/env sh
# test_loop_signal_controls.sh
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

wait_for_process_start() {
    pid="$1"
    retries=0

    while [ "$retries" -lt 40 ]; do
        if kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
        sleep 0.05
        retries=$((retries + 1))
    done

    return 1
}

wait_for_stopped_state() {
    pid="$1"
    retries=0

    while [ "$retries" -lt 80 ]; do
        kill -TSTP "$pid" 2>/dev/null || true
        state=$(ps -o stat= -p "$pid" 2>/dev/null | tr -d ' ')
        case "$state" in
            T*)
                return 0
                ;;
        esac
        sleep 0.05
        retries=$((retries + 1))
    done

    return 1
}

cleanup_pid() {
    pid="$1"

    if [ -z "$pid" ]; then
        return
    fi

    if kill -0 "$pid" 2>/dev/null; then
        kill -CONT "$pid" 2>/dev/null || true
        kill -INT "$pid" 2>/dev/null || true
        sleep 0.05
    fi

    if kill -0 "$pid" 2>/dev/null; then
        kill -KILL "$pid" 2>/dev/null || true
    fi

    wait "$pid" 2>/dev/null || true
}

run_sigint_loop_test() {
    test_name="$1"
    loop_cmd="$2"

    "$CJSH_PATH" -c "sh -c 'sleep 0.2; kill -INT \$PPID' & $loop_cmd" >/dev/null 2>&1
    status=$?

    if [ "$status" -eq 130 ]; then
        pass_test "$test_name"
    else
        fail_test "$test_name (expected exit 130, got $status)"
    fi
}

run_sigtstp_loop_test() {
    test_name="$1"
    loop_cmd="$2"

    "$CJSH_PATH" -c "$loop_cmd" >/dev/null 2>&1 &
    shell_pid=$!

    if ! wait_for_process_start "$shell_pid"; then
        cleanup_pid "$shell_pid"
        fail_test "$test_name (process did not start)"
        return
    fi

    if wait_for_stopped_state "$shell_pid"; then
        pass_test "$test_name"
    else
        fail_test "$test_name (process did not enter stopped state)"
    fi

    cleanup_pid "$shell_pid"
}

run_sigint_abort_multicommand_loop_body_test() {
    test_name="$1"

    script_text='i=0
while [ $i -lt 5 ]; do
  sh -c '\''kill -INT $$'\''
  echo body-$i
  i=$((i+1))
done
echo after'

    output=$(printf "%s\n" "$script_text" | "$CJSH_PATH" 2>/dev/null)
    status=$?

    if [ "$status" -ne 130 ]; then
        fail_test "$test_name (expected exit 130, got $status)"
        return
    fi

    if printf "%s" "$output" | grep -q "after"; then
        fail_test "$test_name (loop continued after SIGINT)"
        return
    fi

    pass_test "$test_name"
}

echo "Test: loop signal controls..."

run_sigint_loop_test "SIGINT interrupts while loop" "while :; do sleep 1; done"
run_sigint_loop_test "SIGINT interrupts until loop" "until false; do sleep 1; done"
run_sigint_loop_test "SIGINT interrupts for loop" "for i in 1 2 3 4 5 6 7 8 9 10; do sleep 1; done"

run_sigtstp_loop_test "SIGTSTP stops while loop" "while :; do sleep 1; done"
run_sigtstp_loop_test "SIGTSTP stops until loop" "until false; do sleep 1; done"
run_sigtstp_loop_test "SIGTSTP stops for loop" "for i in 1 2 3 4 5 6 7 8 9 10; do sleep 1; done"
run_sigint_abort_multicommand_loop_body_test "SIGINT aborts multi-command while body"

echo ""
echo "Loop Signal Control Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ "$TESTS_FAILED" -eq 0 ]; then
    echo "PASS"
    exit 0
fi

echo "FAIL"
exit 1
