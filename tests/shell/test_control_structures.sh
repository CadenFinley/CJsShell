#!/usr/bin/env sh
# Test control structures: if, for, while
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: control structures..."

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

# Test if statement
IF_OUTPUT=$("$CJSH_PATH" -c "if [ 1 -eq 1 ]; then printf ok; else printf fail; fi")
if [ "$IF_OUTPUT" != "ok" ]; then
  fail_test "if statement"
  exit 1
else
  pass_test "if statement"
fi

# Test for loop
FOR_OUTPUT=$("$CJSH_PATH" -c "count=0; for i in 1 2 3; do count=\$((count+i)); done; printf \$count")
if [ "$FOR_OUTPUT" != "6" ]; then
  fail_test "for loop"
  exit 1
else
  pass_test "for loop"
fi

# Test while loop
WHILE_OUTPUT=$("$CJSH_PATH" -c "count=0; i=1; while [ \$i -le 3 ]; do count=\$((count+i)); i=\$((i+1)); done; printf \$count")
if [ "$WHILE_OUTPUT" != "6" ]; then
  fail_test "while loop"
  exit 1
else
  pass_test "while loop"
fi

echo ""
echo "Control Structures Tests Summary:"
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