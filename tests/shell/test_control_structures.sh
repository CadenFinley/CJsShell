#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: control structures..."


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

assert_error_contains() {
  description="$1"
  command_text="$2"
  expected_substring="$3"

  OUTPUT=$("$CJSH_PATH" -c "$command_text" 2>&1)
  STATUS=$?

  if [ $STATUS -eq 0 ]; then
    fail_test "$description (unexpected success)"
    echo "$OUTPUT"
    exit 1
  fi

  if ! printf "%s" "$OUTPUT" | grep -q "$expected_substring"; then
    fail_test "$description (missing message)"
    echo "$OUTPUT"
    exit 1
  fi

  pass_test "$description"
}

IF_OUTPUT=$("$CJSH_PATH" -c "if [ 1 -eq 1 ]; then printf ok; else printf fail; fi")
if [ "$IF_OUTPUT" != "ok" ]; then
  fail_test "if statement"
  exit 1
else
  pass_test "if statement"
fi

FOR_OUTPUT=$("$CJSH_PATH" -c "count=0; for i in 1 2 3; do count=\$((count+i)); done; printf \$count")
if [ "$FOR_OUTPUT" != "6" ]; then
  fail_test "for loop"
  exit 1
else
  pass_test "for loop"
fi

WHILE_OUTPUT=$("$CJSH_PATH" -c "count=0; i=1; while [ \$i -le 3 ]; do count=\$((count+i)); i=\$((i+1)); done; printf \$count")
if [ "$WHILE_OUTPUT" != "6" ]; then
  fail_test "while loop"
  exit 1
else
  pass_test "while loop"
fi

assert_error_contains "inline for missing do" "for i in 1 2 3 do echo \$i" "missing 'do' keyword"
assert_error_contains "multiline for missing done" "for i in 1 2 3; do echo \$i" "missing closing 'done'"
assert_error_contains "for missing iteration list" "for i in do done" "missing iteration list after 'in'"
assert_error_contains "for missing do keyword" "for i in 1 2 3 echo \$i" "missing 'do' keyword"
assert_error_contains "while missing done" "while true; do echo ok" "missing 'done'"
assert_error_contains "while missing condition" "while do echo ok; done" "loop missing condition expression"
assert_error_contains "while missing do keyword" "while true echo ok" "missing 'do' keyword"
assert_error_contains "until missing done" "until false; do echo ok" "missing 'done'"
assert_error_contains "until missing condition" "until do echo ok; done" "loop missing condition expression"
assert_error_contains "if missing fi" "if true; then echo ok" "missing 'fi'"
assert_error_contains "if missing then" "if true echo ok" "missing 'then' keyword"
assert_error_contains "if missing condition" "if then" "missing condition"
assert_error_contains "case missing esac" "case foo in foo) echo ok ;;" "missing 'esac'"
assert_error_contains "case missing in keyword" "case foo foo) echo ok ;; esac" "missing 'in' keyword"

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