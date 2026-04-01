#!/usr/bin/env sh

# test_builtin_dispatch_edge_cases.sh
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

echo "Test: builtin dispatch edge cases..."

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

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

SOURCE_FILE="$TMP_DIR/builtin_dispatch_source.cjsh"
cat > "$SOURCE_FILE" <<'EOF'
echo sourced-via-builtin-dot
EOF

OUT=$("$CJSH_PATH" -c "builtin" 2>&1)
STATUS=$?
if [ $STATUS -eq 2 ] && echo "$OUT" | grep -q "missing command operand"; then
    pass_test "builtin without operand reports usage"
else
    fail_test "builtin missing operand handling (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin builtin" 2>&1)
STATUS=$?
if [ $STATUS -eq 2 ] && echo "$OUT" | grep -q "cannot invoke builtin recursively"; then
    pass_test "builtin rejects recursive invocation"
else
    fail_test "builtin recursion guard (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin ls" 2>&1)
STATUS=$?
if [ $STATUS -eq 1 ] && echo "$OUT" | grep -q "is not a builtin command"; then
    pass_test "builtin rejects external-only commands"
else
    fail_test "builtin external command rejection (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "echo() { printf 'function\\n'; }; builtin echo dispatched")
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "dispatched" ]; then
    pass_test "builtin bypasses function shadowing"
else
    fail_test "builtin function bypass (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "command() { printf 'shadowed\\n'; }; builtin command -v echo")
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "echo" ]; then
    pass_test "builtin can invoke command builtin when shadowed"
else
    fail_test "builtin command dispatch through shadowing (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin [ 7 -gt 3 ]; first=\$?; builtin [ 3 -gt 7 ]; second=\$?; printf '%s:%s' \"\$first\" \"\$second\"")
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "0:1" ]; then
    pass_test "builtin dispatches special [ builtin with statuses"
else
    fail_test "builtin [ dispatch status propagation (status=$STATUS, out='$OUT')"
fi

"$CJSH_PATH" -c "builtin false" >/dev/null 2>&1
STATUS=$?
if [ $STATUS -eq 1 ]; then
    pass_test "builtin propagates wrapped builtin exit status"
else
    fail_test "builtin false status propagation (status=$STATUS)"
fi

OUT=$("$CJSH_PATH" -c "builtin printf '%s:%s' left right")
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "left:right" ]; then
    pass_test "builtin forwards arguments to target builtin"
else
    fail_test "builtin argument forwarding (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin --help" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && echo "$OUT" | grep -q "Usage: builtin COMMAND"; then
    pass_test "builtin --help reports builtin usage"
else
    fail_test "builtin --help output (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin echo --help" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] &&
   echo "$OUT" | grep -q "Usage: echo" &&
   ! echo "$OUT" | grep -q "Usage: builtin"; then
    pass_test "builtin forwards --help to wrapped builtin"
else
    fail_test "builtin wrapped help forwarding (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "builtin . \"$SOURCE_FILE\"" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "sourced-via-builtin-dot" ]; then
    pass_test "builtin dispatches dot builtin"
else
    fail_test "builtin dot dispatch (status=$STATUS, out='$OUT')"
fi

"$CJSH_PATH" -c "builtin :" >/dev/null 2>&1
STATUS=$?
if [ $STATUS -eq 0 ]; then
    pass_test "builtin dispatches null command"
else
    fail_test "builtin null command dispatch (status=$STATUS)"
fi

echo ""
echo "Builtin Dispatch Edge Case Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
