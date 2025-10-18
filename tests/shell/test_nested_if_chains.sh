#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: nested and chained if statements..."

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

# Deeply nested true branches
OUTPUT=$("$CJSH_PATH" -c "
if [ 1 -eq 1 ]; then
    if [ 2 -eq 2 ]; then
        if [ 3 -ne 4 ]; then
            echo 'deep-success'
        else
            echo 'deep-fail'
        fi
    else
        echo 'middle-fail'
    fi
else
    echo 'outer-fail'
fi")
if [ "$OUTPUT" = "deep-success" ]; then
    pass_test "deeply nested true branches"
else
    fail_test "deeply nested true branches (got: '$OUTPUT')"
fi

# Middle branch fallback after nested failure
OUTPUT=$("$CJSH_PATH" -c "
if [ 1 -eq 1 ]; then
    if [ 2 -ne 2 ]; then
        echo 'should-not-see'
    else
        if [ 5 -eq 5 ]; then
            echo 'nested-fallback'
        else
            echo 'nested-fallback-fail'
        fi
    fi
else
    echo 'outer-fail'
fi")
if [ "$OUTPUT" = "nested-fallback" ]; then
    pass_test "nested fallback after inner failure"
else
    fail_test "nested fallback after inner failure (got: '$OUTPUT')"
fi

# Elif with nested decisions
OUTPUT=$("$CJSH_PATH" -c "
if false; then
    echo 'outer-then'
elif [ 3 -eq 3 ]; then
    if false; then
        echo 'inner-then'
    elif [ 4 -gt 3 ]; then
        echo 'chain-match'
    else
        echo 'chain-else'
    fi
else
    echo 'outer-else'
fi")
if [ "$OUTPUT" = "chain-match" ]; then
    pass_test "elif chain with nested inner decisions"
else
    fail_test "elif chain with nested inner decisions (got: '$OUTPUT')"
fi

# Multiple elif chain hitting late branch with nested body
OUTPUT=$("$CJSH_PATH" -c "
if false; then
    echo 'branch-a'
elif false; then
    echo 'branch-b'
elif [ 2 -eq 2 ]; then
    if [ 3 -eq 3 ]; then
        echo 'late-branch'
    else
        echo 'late-branch-fail'
    fi
else
    echo 'branch-default'
fi")
if [ "$OUTPUT" = "late-branch" ]; then
    pass_test "multiple elif chain hitting late nested branch"
else
    fail_test "multiple elif chain hitting late nested branch (got: '$OUTPUT')"
fi

# Nested else block producing multiple outputs
OUTPUT=$("$CJSH_PATH" -c "
if [ 1 -eq 2 ]; then
    echo 'unexpected'
elif [ 3 -eq 4 ]; then
    echo 'unexpected-elif'
else
    if [ 5 -eq 5 ]; then
        echo 'else-branch'
        if [ 6 -lt 7 ]; then
            echo 'else-inner'
        else
            echo 'else-inner-fail'
        fi
    else
        echo 'else-fail'
    fi
fi")
EXPECTED="else-branch
else-inner"
if [ "$OUTPUT" = "$EXPECTED" ]; then
    pass_test "nested else block emits sequential outputs"
else
    fail_test "nested else block emits sequential outputs (got: '$OUTPUT')"
fi

echo
echo "Nested and Chained If Test Summary:"
echo "PASSED: $TESTS_PASSED"
echo "FAILED: $TESTS_FAILED"
echo "SKIPPED: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "All nested if tests passed!"
    exit 0
else
    echo "Some nested if tests failed!"
    exit 1
fi
