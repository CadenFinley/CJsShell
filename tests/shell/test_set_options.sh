#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: Set command options (POSIX compliance gaps)..."
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
echo "Test set -e (errexit) option"
output=$("$CJSH_PATH" -c "set -e; false; echo should_not_print" 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "set -e (errexit) - exits on error"
else
    fail_test "set -e (errexit) - did not exit on error, got: '$output'"
fi
echo "Test set +e (disable errexit) option"
output=$("$CJSH_PATH" -c "set -e; set +e; false; echo should_print" 2>/dev/null)
if [ "$output" = "should_print" ]; then
    pass_test "set +e (disable errexit)"
else
    fail_test "set +e (disable errexit) - got: '$output'"
fi
echo "Test set -u (nounset) option"
output=$("$CJSH_PATH" -c "set -u; echo \$UNDEFINED_VAR" 2>&1)
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass_test "set -u (nounset) - errors on undefined variable"
else
    fail_test "set -u (nounset) - should error on undefined variable, got: '$output'"
fi
echo "Test set +u (allow unset) option"
output=$("$CJSH_PATH" -c "set -u; set +u; echo \$UNDEFINED_VAR" 2>/dev/null)
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass_test "set +u (allow unset) - allows undefined variables"
else
    fail_test "set +u (allow unset) - should allow undefined variables"
fi
echo "Test set -x (xtrace) option"
output=$("$CJSH_PATH" -c "set -x; echo hello" 2>&1)
if echo "$output" | grep -q "echo hello"; then
    pass_test "set -x (xtrace) - prints commands"
else
    fail_test "set -x (xtrace) - should print commands, got: '$output'"
fi
echo "Test set +x (disable xtrace) option"
output=$("$CJSH_PATH" -c "set -x; set +x; echo hello" 2>&1)
if [ "$output" = "hello" ]; then
    pass_test "set +x (disable xtrace)"
else
    if echo "$output" | tail -1 | grep -q "^hello$"; then
        pass_test "set +x (disable xtrace)"
    else
        fail_test "set +x (disable xtrace) - got: '$output'"
    fi
fi
echo "Test set -v (verbose) option"
output=$("$CJSH_PATH" -c "set -v; echo test" 2>&1)
if echo "$output" | grep -q "echo test"; then
    pass_test "set -v (verbose) - prints input lines"
else
    fail_test "set -v (verbose) - should print input lines, got: '$output'"
fi
echo "Test set -n (noexec) option"
output=$("$CJSH_PATH" -c "set -n; echo should_not_execute" 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "set -n (noexec) - doesn't execute commands"
else
    fail_test "set -n (noexec) - should not execute, got: '$output'"
fi
echo "Test set -f (noglob) option"
TEST_DIR="/tmp/cjsh_glob_test_$$"
mkdir -p "$TEST_DIR"
touch "$TEST_DIR/file1.txt" "$TEST_DIR/file2.txt"
output=$("$CJSH_PATH" -c "cd $TEST_DIR && set -f; echo *.txt" 2>/dev/null)
if [ "$output" = "*.txt" ]; then
    pass_test "set -f (noglob) - disables globbing"
else
    fail_test "set -f (noglob) - should not expand glob, got: '$output'"
fi
rm -rf "$TEST_DIR"
echo "Test set +f (enable glob) option"
TEST_DIR="/tmp/cjsh_glob_test2_$$"
mkdir -p "$TEST_DIR"
touch "$TEST_DIR/file1.txt" "$TEST_DIR/file2.txt"
output=$("$CJSH_PATH" -c "cd $TEST_DIR && set -f; set +f; echo *.txt" 2>/dev/null)
if echo "$output" | grep -q "file1.txt" && echo "$output" | grep -q "file2.txt"; then
    pass_test "set +f (enable glob) - re-enables globbing"
else
    fail_test "set +f (enable glob) - should expand globs, got: '$output'"
fi
rm -rf "$TEST_DIR"
echo "Test set -C (noclobber) option"
TEST_FILE="/tmp/cjsh_noclobber_$$"
echo "original" > "$TEST_FILE"
"$CJSH_PATH" -c "set -C; echo new > $TEST_FILE" 2>/dev/null
content=$(cat "$TEST_FILE" 2>/dev/null)
if [ "$content" = "original" ]; then
    pass_test "set -C (noclobber) - prevents overwrite"
else
    fail_test "set -C (noclobber) - should prevent overwrite, got: '$content'"
fi
rm -f "$TEST_FILE"
echo "Test set +C (allow clobber) option"
TEST_FILE="/tmp/cjsh_clobber_$$"
echo "original" > "$TEST_FILE"
"$CJSH_PATH" -c "set -C; set +C; echo new > $TEST_FILE" 2>/dev/null
content=$(cat "$TEST_FILE" 2>/dev/null)
if [ "$content" = "new" ]; then
    pass_test "set +C (allow clobber) - allows overwrite"
else
    fail_test "set +C (allow clobber) - should allow overwrite, got: '$content'"
fi
rm -f "$TEST_FILE"
echo "Test set -o errexit (long form)"
output=$("$CJSH_PATH" -c "set -o errexit; false; echo should_not_print" 2>/dev/null)
if [ -z "$output" ]; then
    pass_test "set -o errexit (long form)"
else
    fail_test "set -o errexit (long form) - got: '$output'"
fi
echo "Test set -o (show all options)"
output=$("$CJSH_PATH" -c "set -o" 2>/dev/null)
if [ -n "$output" ]; then
    pass_test "set -o (show options) - displays option status"
else
    fail_test "set -o (show options) - should display options"
fi
echo ""
echo "================================"
echo "Set Options Summary:"
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "  SKIPPED: $TESTS_SKIPPED"
echo "================================"
if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi
exit 0
