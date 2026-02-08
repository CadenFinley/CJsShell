#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: error and exit codes..."

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

"$CJSH_PATH" -c "exit 42"
if [ $? -ne 42 ]; then
  fail_test "exit 42 returned $?"
  exit 1
else
  pass_test "exit 42"
fi

"$CJSH_PATH" -c "false"
if [ $? -ne 1 ]; then
  fail_test "false returned $?"
  exit 1
else
  pass_test "false command"
fi

echo ""
echo "Error Handling Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
