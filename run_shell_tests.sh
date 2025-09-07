#!/usr/bin/env sh
# Run all shell tests for cjsh
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# Use the built cjsh binary for testing
export CJSH="$SCRIPT_DIR/build/cjsh"
SHELL_TESTS_DIR="$SCRIPT_DIR/tests/shell"
TOTAL=0
PASS=0
FAIL=0

for test in "$SHELL_TESTS_DIR"/test_*.sh; do
  TOTAL=$((TOTAL+1))
  echo "Running $test..."
  sh "$test"
  if [ $? -eq 0 ]; then
    PASS=$((PASS+1))
  else
    FAIL=$((FAIL+1))
  fi
done

echo "Summary: $PASS/$TOTAL tests passed."
exit $FAIL