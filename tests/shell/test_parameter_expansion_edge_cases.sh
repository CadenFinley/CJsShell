#!/usr/bin/env sh

# test_parameter_expansion_edge_cases.sh
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

echo "Test: parameter expansion edge cases..."

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

OUT=$("$CJSH_PATH" -c 'unset x; echo ${x:-$(echo hi)}' 2>&1)
if [ "$OUT" = "hi" ]; then
    pass_test "command substitution in default word"
else
    fail_test "command substitution in default word (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'unset x; echo ${x:-$((1 + 2))}' 2>&1)
if [ "$OUT" = "3" ]; then
    pass_test "arithmetic expansion in default word"
else
    fail_test "arithmetic expansion in default word (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'HOME=/tmp; unset x; echo ${x:-~}' 2>&1)
if [ "$OUT" = "/tmp" ]; then
    pass_test "tilde expansion in default word"
else
    fail_test "tilde expansion in default word (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'x=; echo ${x:=fallback}; echo $x' 2>&1)
EXPECTED="fallback
fallback"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "colon assignment treats null as unset"
else
    fail_test "colon assignment treats null as unset (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'x=; echo ${x:?bad}; echo after' 2>&1)
STATUS=$?
if [ $STATUS -ne 0 ] && ! printf '%s\n' "$OUT" | grep -q '^after$'; then
    pass_test "colon error aborts non-interactive command"
else
    fail_test "colon error should abort (status=$STATUS, got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'if true; then echo ${x:?bad}; fi; echo after' 2>&1)
STATUS=$?
if [ $STATUS -ne 0 ] && ! printf '%s\n' "$OUT" | grep -q '^after$'; then
    pass_test "colon error aborts one-line if body"
else
    fail_test "colon error in if should abort trailing command (status=$STATUS, got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'for i in 1; do echo ${x:?bad}; done; echo after' 2>&1)
STATUS=$?
if [ $STATUS -ne 0 ] && ! printf '%s\n' "$OUT" | grep -q '^after$'; then
    pass_test "colon error aborts one-line for body"
else
    fail_test "colon error in for should abort trailing command (status=$STATUS, got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'i=0; while [ $i -lt 1 ]; do i=$((i + 1)); echo ${x:?bad}; done; echo after' 2>&1)
STATUS=$?
if [ $STATUS -ne 0 ] && ! printf '%s\n' "$OUT" | grep -q '^after$'; then
    pass_test "colon error aborts one-line while body"
else
    fail_test "colon error in while should abort trailing command (status=$STATUS, got '$OUT')"
fi

echo ""
echo "Parameter Expansion Edge Cases Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
