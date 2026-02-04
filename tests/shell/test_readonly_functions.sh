#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: readonly -f (function readonly support)..."

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

OUT=$("$CJSH_PATH" -c 'foo() { echo one; }; readonly -f foo; readonly -f' 2>/dev/null)
if echo "$OUT" | grep -q "readonly -f foo"; then
    pass_test "readonly -f lists readonly functions"
else
    fail_test "readonly -f list missing (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'readonly -f does_not_exist' 2>/dev/null; echo $?)
if [ "$OUT" != "0" ]; then
    pass_test "readonly -f fails for unknown function"
else
    fail_test "readonly -f should fail for unknown function"
fi

OUT=$("$CJSH_PATH" -c 'foo() { echo one; }; readonly -f foo; foo; foo() { echo two; }; status=$?; foo; echo status:$status' 2>/dev/null)
EXPECTED="one
one
status:1"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "readonly -f prevents function redefinition"
else
    fail_test "readonly -f redefine (got '$OUT', expected '$EXPECTED')"
fi

echo ""
echo "Readonly Function Tests Summary:"
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
