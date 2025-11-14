#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: pipeline functions"

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

OUTPUT=$("$CJSH_PATH" -c '
produce() { printf "%s\n" foo; }
produce | wc -l
' 2>/dev/null | tr -d '[:space:]')
if [ "$OUTPUT" = "1" ]; then
    pass_test "function can start pipeline"
else
    fail_test "function pipeline start - expected '1', got '$OUTPUT'"
fi

MID_OUTPUT=$("$CJSH_PATH" -c '
upper() { tr "[:lower:]" "[:upper:]"; }
printf "hello\nworld\n" | upper | head -n1
' 2>/dev/null | tr -d '\r')
MID_OUTPUT_TRIM=$(printf "%s" "$MID_OUTPUT" | tr -d '\n')
if [ "$MID_OUTPUT_TRIM" = "HELLO" ]; then
    pass_test "function executes in middle of pipeline"
else
    fail_test "function in middle of pipeline - expected 'HELLO', got '$MID_OUTPUT_TRIM'"
fi

PIPESTATUS_RESULT=$("$CJSH_PATH" -c '
fail_stage() { printf "%s" foo; return 5; }
uppercase_stage() { tr "[:lower:]" "[:upper:]"; }
fail_stage | uppercase_stage >/dev/null
pipeline_exit=$?
echo "$PIPESTATUS"
echo "$pipeline_exit"
' 2>&1)
PIPESTATUS_LINE=$(printf "%s" "$PIPESTATUS_RESULT" | head -n1 | tr -d '\r')
PIPESTATUS_LINE=$(echo "$PIPESTATUS_LINE" | sed 's/^ *//; s/ *$//; s/  */ /g')
PIPE_EXIT_LINE=$(printf "%s" "$PIPESTATUS_RESULT" | tail -n1 | tr -d '[:space:]\r')
if [ "$PIPESTATUS_LINE" = "5 0" ] && [ "$PIPE_EXIT_LINE" = "0" ]; then
    pass_test "PIPESTATUS records function stage exit"
else
    fail_test "PIPESTATUS with function pipeline - got '$PIPESTATUS_LINE' and exit '$PIPE_EXIT_LINE'"
    echo "$PIPESTATUS_RESULT"
fi

FINAL_RESULT=$("$CJSH_PATH" -c '
consumer() {
    read -r line || return 1
    return 3
}
printf "data\n" | consumer >/dev/null
pipeline_exit=$?
echo "$PIPESTATUS"
echo "$pipeline_exit"
' 2>&1)
FINAL_STATUS_LINE=$(printf "%s" "$FINAL_RESULT" | head -n1 | tr -d '\r')
FINAL_STATUS_LINE=$(echo "$FINAL_STATUS_LINE" | sed 's/^ *//; s/ *$//; s/  */ /g')
FINAL_EXIT_LINE=$(printf "%s" "$FINAL_RESULT" | tail -n1 | tr -d '[:space:]\r')
if [ "$FINAL_STATUS_LINE" = "0 3" ] && [ "$FINAL_EXIT_LINE" = "3" ]; then
    pass_test "function exit status propagates as last pipeline stage"
else
    fail_test "function as pipeline sink - got '$FINAL_STATUS_LINE' and exit '$FINAL_EXIT_LINE'"
    echo "$FINAL_RESULT"
fi

echo ""
echo "Pipeline Function Tests Summary:"
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
