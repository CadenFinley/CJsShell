#!/usr/bin/env sh

# test_process_management.sh
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

"$CJSH_PATH" -c "true"
if [ $? -ne 0 ]; then
    fail_test "true command should exit with 0"
    exit 1
else
    pass_test "true command"
fi

"$CJSH_PATH" -c "false"
if [ $? -ne 1 ]; then
    fail_test "false command should exit with 1"
    exit 1
else
    pass_test "false command"
fi

OUT=$("$CJSH_PATH" -c "false; echo \$?")
if [ "$OUT" != "1" ]; then
    fail_test "exit status propagation (got '$OUT')"
    exit 1
else
    pass_test "exit status propagation"
fi

OUT=$("$CJSH_PATH" -c "echo hello | wc -w" | tr -d ' ')
if [ "$OUT" != "1" ]; then
    fail_test "process substitution (got '$OUT')"
    exit 1
else
    pass_test "process substitution"
fi

OUT=$("$CJSH_PATH" -c "echo first; echo second")
EXPECTED="first
second"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "multiple commands with semicolon (got '$OUT')"
    exit 1
else
    pass_test "multiple commands with semicolon"
fi

"$CJSH_PATH" -c "sleep 0.1 &"
if [ $? -ne 0 ]; then
    fail_test "background process execution"
    exit 1
else
    pass_test "background process execution"
fi

"$CJSH_PATH" -c "nonexistent_command_12345" 2>/dev/null
if [ $? -eq 0 ]; then
    fail_test "nonexistent command should return non-zero"
    exit 1
else
    pass_test "nonexistent command handling"
fi

timeout 1 "$CJSH_PATH" -c "sleep 2" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    fail_test "timeout should interrupt long-running command"
    exit 1
else
    pass_test "timeout handling"
fi

OUT=$("$CJSH_PATH" -c "exec echo 'exec test'")
if [ "$OUT" != "exec test" ]; then
    fail_test "exec command (got '$OUT')"
    exit 1
else
    pass_test "exec command"
fi

"$CJSH_PATH" -c "exit 42"
if [ $? -ne 42 ]; then
    fail_test "exit builtin with custom code"
    exit 1
else
    pass_test "exit builtin with custom code"
fi

export TEST_PROC_VAR="inherited"
OUT=$("$CJSH_PATH" -c "echo \$TEST_PROC_VAR")
if [ "$OUT" != "inherited" ]; then
    fail_test "process environment inheritance (got '$OUT')"
    exit 1
else
    pass_test "process environment inheritance"
fi

OUT=$("$CJSH_PATH" -c "echo \$(echo substituted)")
if [ "$OUT" != "substituted" ]; then
    fail_test "command substitution in process (got '$OUT')"
    exit 1
else
    pass_test "command substitution in process"
fi

OUT=$("$CJSH_PATH" -c "echo \$(echo \$(echo nested))")
if [ "$OUT" != "nested" ]; then
    fail_test "nested command execution (got '$OUT')"
    exit 1
else
    pass_test "nested command execution"
fi

OUT=$("$CJSH_PATH" -c "which echo")
if [ -z "$OUT" ]; then
    fail_test "PATH resolution for which command"
    exit 1
else
    pass_test "PATH resolution"
fi

echo ""
echo "Process Management Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
