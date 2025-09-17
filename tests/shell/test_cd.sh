#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
  CJSH_PATH="$CJSH"
else
  CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: builtin cd..."

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

# Test cd to /tmp
OUTPUT=$("$CJSH_PATH" -c "cd /tmp; pwd")
if [ "$OUTPUT" = "/tmp" ] || [ "$OUTPUT" = "/private/tmp" ]; then
  pass_test "cd to /tmp"
else
  fail_test "cd to /tmp - expected '/tmp', got '$OUTPUT'"
  exit 1
fi

echo ""
echo "CD Tests Summary:"
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