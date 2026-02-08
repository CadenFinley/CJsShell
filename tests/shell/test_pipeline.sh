#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: pipelines..."

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

OUTPUT=$("$CJSH_PATH" -c "printf 'hello' | sed s/hello/world/ | wc -c")
OUTPUT=$(echo "$OUTPUT" | tr -d '[:space:]')
if [ "$OUTPUT" = "5" ]; then
  pass_test "basic pipeline"
else
  fail_test "basic pipeline - expected '5', got '$OUTPUT'"
  exit 1
fi

"$CJSH_PATH" -c "! true" >/dev/null 2>&1
EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 1 ]; then
    pass_test "! true yields exit 1"
else
    fail_test "! true should exit 1 but exited $EXIT_CODE"
fi

"$CJSH_PATH" -c "! false" >/dev/null 2>&1
EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 0 ]; then
    pass_test "! false yields exit 0"
else
    fail_test "! false should exit 0 but exited $EXIT_CODE"
fi

"$CJSH_PATH" -c "! true | false" >/dev/null 2>&1
EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 0 ]; then
    pass_test "! true | false yields exit 0"
else
    fail_test "! true | false should exit 0 but exited $EXIT_CODE"
fi

"$CJSH_PATH" -c "! false | true" >/dev/null 2>&1
EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 1 ]; then
    pass_test "! false | true yields exit 1"
else
    fail_test "! false | true should exit 1 but exited $EXIT_CODE"
fi

echo ""
echo "Pipeline Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
