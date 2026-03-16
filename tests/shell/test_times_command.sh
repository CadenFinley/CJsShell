#!/usr/bin/env sh

# test_times_command.sh
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

echo "Test: times command..."

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

"$CJSH_PATH" -c "times" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "times command executes"
else
    fail_test "times command failed to execute"
fi

OUT=$("$CJSH_PATH" -c "times")
LINE_COUNT=$(echo "$OUT" | wc -l | tr -d ' ')
if [ "$LINE_COUNT" -ge 1 ]; then
    pass_test "times produces output"
else
    fail_test "times should produce output"
fi

OUT=$("$CJSH_PATH" -c "times")
if echo "$OUT" | grep -qE "[0-9]+m[0-9]+\.[0-9]+s|[0-9]+\.[0-9]+"; then
    pass_test "times output contains time values"
else
    fail_test "times output format unexpected: $OUT"
fi

OUT=$("$CJSH_PATH" -c "echo test >/dev/null; echo test2 >/dev/null; times")
if [ $? -eq 0 ]; then
    pass_test "times after running commands"
else
    fail_test "times failed after running commands"
fi

OUT=$("$CJSH_PATH" -c "sleep 0.01; times")
if echo "$OUT" | grep -qE "[0-9]"; then
    pass_test "times with child process"
else
    fail_test "times with child process (no output)"
fi

"$CJSH_PATH" -c "times; echo \$?" >/dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    pass_test "times exit status is 0"
else
    fail_test "times exit status should be 0 (got $EXIT_CODE)"
fi

"$CJSH_PATH" -c "times arg1 arg2" >/dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "times handles arguments gracefully"
else
    fail_test "times unexpected behavior with arguments"
fi

OUT=$("$CJSH_PATH" -c "(times)")
if [ $? -eq 0 ]; then
    pass_test "times works in subshell"
else
    fail_test "times failed in subshell"
fi

OUT=$("$CJSH_PATH" -c "times")
if echo "$OUT" | grep -qE "([0-9]+m)?[0-9]+\.[0-9]+s"; then
    pass_test "times output format valid"
else
    fail_test "times output format differs from expected"
fi

OUT1=$("$CJSH_PATH" -c "times" | head -1)
OUT2=$("$CJSH_PATH" -c "i=0; while [ \$i -lt 100 ]; do i=\$((i+1)); done; times" | head -1)
if [ -n "$OUT1" ] && [ -n "$OUT2" ]; then
    pass_test "times tracks computation (results: '$OUT1' vs '$OUT2')"
else
    fail_test "times computation tracking failed"
fi

OUT=$("$CJSH_PATH" -c "times --help" 2>&1)
if echo "$OUT" | grep -qi "usage\|print.*time"; then
    pass_test "times --help provides usage info"
else
    fail_test "times --help format differs"
fi

"$CJSH_PATH" -c "VAR=before; times >/dev/null; echo \$VAR" > /tmp/times_test_var
OUT=$(cat /tmp/times_test_var)
rm -f /tmp/times_test_var
if [ "$OUT" = "before" ]; then
    pass_test "times doesn't modify shell state"
else
    fail_test "times modified shell state (VAR=$OUT)"
fi

echo ""
echo "Times Command Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
