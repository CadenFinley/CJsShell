#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: logical AND/OR..."

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

# Test logical AND with true
OUT1=$("$CJSH_PATH" -c "true && echo ok")
if [ "$OUT1" != "ok" ]; then
  fail_test "true && echo ok -> '$OUT1'"
  exit 1
else
  pass_test "logical AND with true"
fi

# Test logical OR with false
OUT2=$("$CJSH_PATH" -c "false || echo ok")
if [ "$OUT2" != "ok" ]; then
  fail_test "false || echo ok -> '$OUT2'"
  exit 1
else
  pass_test "logical OR with false"
fi

# Test logical AND with false
"$CJSH_PATH" -c "false && echo nope"
if [ $? -eq 0 ]; then
  fail_test "false && echo nope should not succeed"
  exit 1
else
  pass_test "logical AND with false"
fi

echo ""
echo "Logical AND/OR Tests Summary:"
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
