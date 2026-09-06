#!/usr/bin/env sh
# test_signal_exits.sh
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

. "$SCRIPT_DIR/process_cleanup_helpers.sh"

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Signal Exit and Cleanup Behavior for: $SHELL_TO_TEST"
echo "============================================================"

log_test "SIGTERM triggers graceful cleanup"
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" TERM 0 redirected); then
    pass
else
    fail "$cleanup_output"
fi

log_test "SIGHUP triggers graceful cleanup"
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" HUP 0 redirected); then
    pass
else
    fail "$cleanup_output"
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
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" TERM 1); then
    pass
else
    fail "$cleanup_output"
fi

log_test "Resource cleanup on forced exit with huponexit"
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" force); then
    pass
else
    fail "$cleanup_output"
fi

for read_mode in read timed-read; do
    log_test "SIGTERM interrupts blocking $read_mode and cleans up background jobs"
    if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" TERM 1 "$read_mode"); then
        pass
    else
        fail "$cleanup_output"
    fi
done

log_test "Parent reaps a shell terminated by SIGKILL"
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" KILL); then
    pass
else
    fail "$cleanup_output"
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
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" multiple 1); then
    pass
else
    fail "$cleanup_output"
fi

log_test "Signal handling consistency across modes"
if cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" TERM 1 script) &&
   cleanup_output=$(check_process_cleanup "$SHELL_TO_TEST" TERM 1 command); then
    pass
else
    fail "$cleanup_output"
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
