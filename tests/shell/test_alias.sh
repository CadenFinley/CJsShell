#!/usr/bin/env sh

# test_alias.sh
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

if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: alias/unalias..."

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

OUT=$("$CJSH_PATH" -c "alias hi='echo hello'; hi")
if [ "$OUT" != "hello" ]; then
  fail_test "alias expansion simple (got '$OUT')"
  exit 1
else
  pass_test "alias expansion simple"
fi

OUT2=$("$CJSH_PATH" -c "alias say='echo'; say world")
if [ "$OUT2" != "world" ]; then
  fail_test "alias with args (got '$OUT2')"
  exit 1
else
  pass_test "alias with args"
fi

OUT3=$("$CJSH_PATH" -c "alias hi='echo hello'; unalias hi; command -v hi >/dev/null 2>&1; echo \$?" 2>/dev/null)
if [ "$OUT3" = "0" ]; then
  fail_test "unalias did not remove alias"
  exit 1
else
  pass_test "unalias removes alias"
fi

OUT_PIPE=$("$CJSH_PATH" -c "alias shout='echo hello'; shout | tr 'a-z' 'A-Z'")
if [ "$OUT_PIPE" != "HELLO" ]; then
  fail_test "alias not expanded in pipeline (got '$OUT_PIPE')"
  exit 1
else
  pass_test "alias expands inside pipeline"
fi

TMP_FILE=$(mktemp)
"$CJSH_PATH" -c "alias saver='echo hello'; saver > \"$TMP_FILE\""
OUT_RED=$(cat "$TMP_FILE")
rm -f "$TMP_FILE"
if [ "$OUT_RED" != "hello" ]; then
  fail_test "alias not expanded with redirection (got '$OUT_RED')"
  exit 1
else
  pass_test "alias expands with redirection"
fi

echo ""
echo "Alias Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
