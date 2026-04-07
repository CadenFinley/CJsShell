#!/usr/bin/env sh

# test_builtin_strict_compat.sh
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

echo "Test: strict builtin bash-compat checks..."

if ! command -v bash >/dev/null 2>&1; then
    echo "WARNING: bash is unavailable; skipping strict compatibility checks"
    exit 0
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

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

REAL_DIR="$TMP_DIR/real"
LINK_DIR="$TMP_DIR/link"
mkdir -p "$REAL_DIR"
ln -s "$REAL_DIR" "$LINK_DIR"

echo "Check: cd default keeps logical path"
EXPECTED=$(LINK_DIR="$LINK_DIR" bash -lc 'cd "$LINK_DIR"; printf "%s" "$PWD"' 2>/dev/null)
ACTUAL=$(LINK_DIR="$LINK_DIR" "$CJSH_PATH" -c 'cd "$LINK_DIR"; printf "%s" "$PWD"' 2>/dev/null)
if [ "$ACTUAL" = "$EXPECTED" ]; then
    pass_test "cd default logical mode"
else
    fail_test "cd default logical mode (expected '$EXPECTED', got '$ACTUAL')"
fi

echo "Check: cd -P resolves to physical path"
EXPECTED=$(LINK_DIR="$LINK_DIR" bash -lc 'cd -P "$LINK_DIR"; printf "%s" "$PWD"' 2>/dev/null)
ACTUAL=$(LINK_DIR="$LINK_DIR" "$CJSH_PATH" -c 'cd -P "$LINK_DIR"; printf "%s" "$PWD"' 2>/dev/null)
if [ "$ACTUAL" = "$EXPECTED" ]; then
    pass_test "cd -P physical mode"
else
    fail_test "cd -P physical mode (expected '$EXPECTED', got '$ACTUAL')"
fi

echo "Check: pwd default output matches bash logical mode"
EXPECTED=$(LINK_DIR="$LINK_DIR" bash -lc 'cd "$LINK_DIR"; pwd' 2>/dev/null)
ACTUAL=$(LINK_DIR="$LINK_DIR" "$CJSH_PATH" -c 'cd "$LINK_DIR"; pwd' 2>/dev/null)
if [ "$ACTUAL" = "$EXPECTED" ]; then
    pass_test "pwd default logical output"
else
    fail_test "pwd default logical output (expected '$EXPECTED', got '$ACTUAL')"
fi

echo "Check: type -P resolves executable path"
EXPECTED=$(bash -lc 'type -P ls' 2>/dev/null)
ACTUAL=$("$CJSH_PATH" -c 'type -P ls' 2>/dev/null)
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$ACTUAL" = "$EXPECTED" ]; then
    pass_test "type -P executable resolution"
else
    fail_test "type -P executable resolution (status=$STATUS, expected '$EXPECTED', got '$ACTUAL')"
fi

echo "Check: export -p succeeds"
"$CJSH_PATH" -c 'export -p >/dev/null' >/dev/null 2>&1
STATUS=$?
if [ $STATUS -eq 0 ]; then
    pass_test "export -p support"
else
    fail_test "export -p support (status=$STATUS)"
fi

echo "Check: export NAME marks unset variable for export"
OUT=$(
    "$CJSH_PATH" -c '
        unset CJSH_STRICT_EXPORT
        export CJSH_STRICT_EXPORT
        export_status=$?
        /usr/bin/env | grep "^CJSH_STRICT_EXPORT=" >/dev/null
        grep_status=$?
        printf "%s:%s" "$export_status" "$grep_status"
    ' 2>/dev/null
)
if [ "$OUT" = "0:0" ]; then
    pass_test "export NAME on unset variable"
else
    fail_test "export NAME on unset variable (expected '0:0', got '$OUT')"
fi

echo "Check: alias -p prints aliases"
OUT=$(
    "$CJSH_PATH" -c "
        alias compat_alias='printf ok'
        alias -p
    " 2>/dev/null
)
if printf "%s" "$OUT" | grep -q "alias compat_alias='printf ok'"; then
    pass_test "alias -p support"
else
    fail_test "alias -p support (output='$OUT')"
fi

echo "Check: unalias -a clears all aliases"
OUT=$(
    "$CJSH_PATH" -c "
        alias compat_a='echo a'
        alias compat_b='echo b'
        unalias -a
        alias
    " 2>/dev/null
)
if printf "%s" "$OUT" | grep -q "No aliases defined."; then
    pass_test "unalias -a support"
else
    fail_test "unalias -a support (output='$OUT')"
fi

echo "Check: getopts requires NAME operand"
"$CJSH_PATH" -c 'getopts ab' >/dev/null 2>&1
STATUS=$?
if [ $STATUS -eq 2 ]; then
    pass_test "getopts missing NAME status"
else
    fail_test "getopts missing NAME status (expected 2, got $STATUS)"
fi

echo "Check: test -a works as file-exists unary operator"
REAL_DIR="$REAL_DIR" "$CJSH_PATH" -c 'test -a "$REAL_DIR"' >/dev/null 2>&1
STATUS=$?
if [ $STATUS -eq 0 ]; then
    pass_test "test -a unary operator"
else
    fail_test "test -a unary operator (status=$STATUS)"
fi

echo "Check: exec replaces shell process"
OUT=$(
    "$CJSH_PATH" -c 'exec /bin/sh -c "printf replaced"; printf after' 2>/dev/null
)
if [ "$OUT" = "replaced" ]; then
    pass_test "exec process replacement"
else
    fail_test "exec process replacement (expected 'replaced', got '$OUT')"
fi

echo ""
echo "================================"
echo "Strict Builtin Compatibility Summary:"
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "================================"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi

exit 0
