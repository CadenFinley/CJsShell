#!/usr/bin/env sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: --no-error-suggestions and --minimal..."

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

run_cjsh() {
    "$CJSH_PATH" "$@" </dev/null 2>&1
}

baseline_output=$(run_cjsh -c "ech")
if printf "%s\n" "$baseline_output" | grep -q "Did you mean:"; then
    pass_test "default suggests for mistyped commands"
else
    fail_test "default should suggest for mistyped commands"
fi

no_suggestions_output=$(run_cjsh --no-error-suggestions -c "ech")
if printf "%s\n" "$no_suggestions_output" | grep -q "Did you mean:"; then
    fail_test "--no-error-suggestions should suppress suggestions"
else
    pass_test "--no-error-suggestions suppresses suggestions"
fi

minimal_output=$(run_cjsh --minimal -c "ech")
if printf "%s\n" "$minimal_output" | grep -q "Did you mean:"; then
    fail_test "--minimal should suppress suggestions"
else
    pass_test "--minimal suppresses suggestions"
fi

echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))
if [ $TESTS_FAILED -eq 0 ]; then
    echo "All tests passed! ($TESTS_PASSED/$TOTAL_TESTS)"
    exit 0
else
    echo "Some tests failed. ($TESTS_PASSED/$TOTAL_TESTS)"
    exit 1
fi
