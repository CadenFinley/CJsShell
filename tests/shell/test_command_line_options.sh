#!/usr/bin/env sh

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: command line options..."

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

OUT=$("$CJSH_PATH" -c "echo test-command")
if [ "$OUT" != "test-command" ]; then
    fail_test "-c option (got '$OUT')"
    exit 1
else
    pass_test "-c option"
fi

OUT=$("$CJSH_PATH" -v 2>/dev/null)
if [ -z "$OUT" ]; then
    fail_test "-v option should output version"
    exit 1
else
    pass_test "-v option"
fi

OUT=$("$CJSH_PATH" --version 2>/dev/null)
if [ -z "$OUT" ]; then
    fail_test "--version option should output version"
    exit 1
else
    pass_test "--version option"
fi

OUT=$("$CJSH_PATH" -h 2>/dev/null)
if [ -z "$OUT" ]; then
    fail_test "-h option should output help"
    exit 1
else
    pass_test "-h option"
fi

OUT=$("$CJSH_PATH" --help 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "FAIL: --help option should output help"
    exit 1
fi

OUT=$("$CJSH_PATH" --no-colors -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-colors option (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" --no-colors --no-themes -c "echo multi-test")
if [ "$OUT" != "multi-test" ]; then
    echo "FAIL: multiple options (got '$OUT')"
    exit 1
fi

"$CJSH_PATH" --invalid-option 2>/dev/null
if [ $? -eq 0 ]; then
    echo "FAIL: invalid option should return non-zero exit code"
    exit 1
fi

"$CJSH_PATH" --startup-test 2>/dev/null
if [ $? -ne 0 ]; then
    fail_test "--startup-test should complete successfully"
    exit 1
else
    pass_test "--startup-test"
fi


echo ""
echo "Command Line Options Tests Summary:"
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
