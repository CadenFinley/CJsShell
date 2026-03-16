#!/usr/bin/env sh

# test_readonly_functions.sh
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

echo "Test: readonly -f (function readonly support)..."

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

OUT=$("$CJSH_PATH" -c 'foo() { echo one; }; readonly -f foo; readonly -f' 2>/dev/null)
if echo "$OUT" | grep -q "readonly -f foo"; then
    pass_test "readonly -f lists readonly functions"
else
    fail_test "readonly -f list missing (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'readonly -f does_not_exist' 2>/dev/null; echo $?)
if [ "$OUT" != "0" ]; then
    pass_test "readonly -f fails for unknown function"
else
    fail_test "readonly -f should fail for unknown function"
fi

OUT=$("$CJSH_PATH" -c 'foo() { echo one; }; readonly -f foo; foo; foo() { echo two; }; status=$?; foo; echo status:$status' 2>/dev/null)
EXPECTED="one
one
status:1"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "readonly -f prevents function redefinition"
else
    fail_test "readonly -f redefine (got '$OUT', expected '$EXPECTED')"
fi

echo ""
echo "Readonly Function Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
