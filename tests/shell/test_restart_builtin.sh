#!/usr/bin/env sh

# test_restart_builtin.sh
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

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
elif [ -x "$SCRIPT_DIR/../../build/cjsh" ]; then
    CJSH_PATH="$SCRIPT_DIR/../../build/cjsh"
elif [ -x "$SCRIPT_DIR/../../build/release/cjsh" ]; then
    CJSH_PATH="$SCRIPT_DIR/../../build/release/cjsh"
else
    CJSH_PATH="$SCRIPT_DIR/../../build/cjsh"
fi

echo "Test: restart builtin..."

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
if [ -z "$CJSH_RESTART_BASIC_ONCE" ]; then
    export CJSH_RESTART_BASIC_ONCE=1
    export CJSH_RESTART_BASIC_SHLVL="$SHLVL"
    restart
fi

if [ "$SHLVL" -gt "$CJSH_RESTART_BASIC_SHLVL" ]; then
    echo restarted
else
    echo not-restarted:"$SHLVL":"$CJSH_RESTART_BASIC_SHLVL"
fi
' 2>/dev/null)
if [ "$OUT" = "restarted" ]; then
    pass_test "restart re-execs the shell process"
else
    fail_test "restart re-execs the shell process (got '$OUT')"
fi

OUT=$("$CJSH_PATH" --posix -c 'restart' 2>&1)
STATUS=$?
if [ "$STATUS" -ne 0 ] && printf "%s" "$OUT" | grep -Fq "not available in POSIX mode"; then
    pass_test "restart is disabled in POSIX mode"
else
    fail_test "restart is disabled in POSIX mode (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" --posix -c 'restart --no-flags' 2>&1)
STATUS=$?
if [ "$STATUS" -ne 0 ] && printf "%s" "$OUT" | grep -Fq "not available in POSIX mode"; then
    pass_test "restart --no-flags is disabled in POSIX mode"
else
    fail_test "restart --no-flags is disabled in POSIX mode (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" --posix -c 'restart -n' 2>&1)
STATUS=$?
if [ "$STATUS" -ne 0 ] && printf "%s" "$OUT" | grep -Fq "not available in POSIX mode"; then
    pass_test "restart -n is disabled in POSIX mode"
else
    fail_test "restart -n is disabled in POSIX mode (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "restart --help" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 0 ] && printf "%s" "$OUT" | grep -Fq "Usage: restart" &&
   printf "%s" "$OUT" | grep -Fq -- "--no-flags"; then
    pass_test "restart --help prints usage"
else
    fail_test "restart --help prints usage (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "restart --not-a-real-option" 2>&1)
STATUS=$?
if [ "$STATUS" -ne 0 ] && printf "%s" "$OUT" | grep -Fq "invalid option"; then
    pass_test "restart rejects unknown options"
else
    fail_test "restart rejects unknown options (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "restart now" 2>&1)
STATUS=$?
if [ "$STATUS" -ne 0 ] && printf "%s" "$OUT" | grep -Fq "unexpected argument"; then
    pass_test "restart rejects positional arguments"
else
    fail_test "restart rejects positional arguments (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "restart -- after" 2>&1)
STATUS=$?
if [ "$STATUS" -ne 0 ] && printf "%s" "$OUT" | grep -Fq "unexpected argument"; then
    pass_test "restart handles -- with trailing arguments"
else
    fail_test "restart handles -- with trailing arguments (status=$STATUS, output='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin restart --help" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 0 ] && printf "%s" "$OUT" | grep -Fq "Usage: restart"; then
    pass_test "restart works through builtin dispatcher"
else
    fail_test "restart works through builtin dispatcher (status=$STATUS, output='$OUT')"
fi

echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
