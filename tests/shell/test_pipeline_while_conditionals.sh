#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: pipeline while loops inside conditionals"

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

PIPELINE_SCRIPT='run_block() {
    if printf "%s\n" "warn" | grep -q "warn"; then
        data=$(printf "%s\n" warn info)
        if printf "%s\n" "$data" | grep -q "warn"; then
            printf "%s\n" "$data" | while IFS= read -r line; do
                if [ -n "$line" ]; then
                    echo "line:$line"
                fi
            done
        fi
    fi
}

run_block
'

OUTPUT=$("$CJSH_PATH" -c "$PIPELINE_SCRIPT" 2>&1)
EXIT_CODE=$?

if [ "$EXIT_CODE" -eq 0 ]; then
    echo "$OUTPUT" | grep -q "line:warn"
    HAS_WARN=$?
    echo "$OUTPUT" | grep -q "line:info"
    HAS_INFO=$?
    echo "$OUTPUT" | grep -q "SYN001"
    HAS_SYNTAX=$?
    if [ "$HAS_WARN" -eq 0 ] && [ "$HAS_INFO" -eq 0 ] && [ "$HAS_SYNTAX" -ne 0 ]; then
        pass_test "pipeline while loop inside nested conditionals executes"
    else
        fail_test "pipeline while loop inside nested conditionals should run successfully"
        echo "$OUTPUT"
    fi
else
    fail_test "pipeline while loop script exited with $EXIT_CODE"
    echo "$OUTPUT"
fi

INLINE_SCRIPT='for i in 1 2 3; do printf "%s " "$i"; done'
INLINE_OUTPUT=$("$CJSH_PATH" -c "$INLINE_SCRIPT" 2>&1)
INLINE_EXIT=$?

if [ "$INLINE_EXIT" -eq 0 ] && [ "$INLINE_OUTPUT" = "1 2 3 " ]; then
    pass_test "inline for loop closes correctly"
else
    fail_test "inline for loop should produce '1 2 3 '"
    echo "$INLINE_OUTPUT"
fi

echo ""
echo "Pipeline While Conditional Tests Summary:"
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
