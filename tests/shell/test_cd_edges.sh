#!/usr/bin/env sh
# Test 'cd' edge cases: non-existent dir, HOME
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: cd edge cases..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

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

# Test cd to non-existent directory
"$CJSH_PATH" -c "cd /definitely/not/a/real/path"
if [ $? -eq 0 ]; then
  fail_test "cd to non-existent path should fail"
  exit 1
else
  pass_test "cd to non-existent path should fail"
fi

# Test cd to HOME directory
HOME_OUT=$("$CJSH_PATH" -c "cd; pwd")
HOME_OS=${HOME#/private}
HOME_OUT_OS=${HOME_OUT#/private}
if [ "$HOME_OUT_OS" != "$HOME_OS" ]; then
  fail_test "cd to HOME expected '$HOME', got '$HOME_OUT'"
  exit 1
else
  pass_test "cd to HOME directory"
fi

echo ""
echo "CD Edge Cases Tests Summary:"
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
