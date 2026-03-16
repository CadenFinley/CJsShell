#!/usr/bin/env sh

# test_refactor_regressions.sh
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

echo "Test: refactor regression coverage..."

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

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t cjsh_refactor_tests)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

HISTORY_FILE="$TMP_DIR/history.txt"
cat >"$HISTORY_FILE" <<'EOF'
# metadata line

echo one
echo two
# ignored comment
echo three
EOF

OUT=$(CJSH_HISTORY_FILE="$HISTORY_FILE" "$CJSH_PATH" -c 'history' 2>/dev/null)
if echo "$OUT" | grep -q "echo one" && echo "$OUT" | grep -q "echo two" &&
   echo "$OUT" | grep -q "echo three" && ! echo "$OUT" | grep -q "metadata line"; then
    pass_test "history filters comments and blank lines"
else
    fail_test "history filtering regression (got '$OUT')"
fi

OUT=$(CJSH_HISTORY_FILE="$HISTORY_FILE" "$CJSH_PATH" -c 'history nope' >/dev/null 2>&1; echo $?)
if [ "$OUT" = "1" ]; then
    pass_test "history rejects invalid count"
else
    fail_test "history invalid count exit code changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'readonly alpha=beta; readonly' 2>/dev/null)
if echo "$OUT" | grep -q "readonly alpha=beta"; then
    pass_test "readonly default listing format"
else
    fail_test "readonly default listing changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'readonly alpha=beta; readonly -p' 2>/dev/null)
if echo "$OUT" | grep -q "readonly alpha='beta'"; then
    pass_test "readonly -p listing format"
else
    fail_test "readonly -p listing changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "trap 'echo trap_hit' EXIT; trap -p" 2>/dev/null)
if echo "$OUT" | grep -q "trap -- 'echo trap_hit' EXIT"; then
    pass_test "trap -p listing format"
else
    fail_test "trap -p listing changed (got '$OUT')"
fi

echo ""
echo "Refactor Regression Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
