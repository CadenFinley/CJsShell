#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: alias/unalias..."

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
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

echo ""
echo "Alias Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
