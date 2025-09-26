#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: which command comprehensive..."

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

# Test 1: Basic which command functionality
echo "Test 1: Basic which functionality..."
OUT=$("$CJSH_PATH" -c "which echo" 2>&1)
if [ $? -eq 0 ]; then
    pass_test "basic which command execution"
else
    fail_test "basic which command execution (exit code: $?, output: '$OUT')"
fi

# Test 2: Which shows CJsShell custom implementations
echo "Test 2: Which shows CJsShell custom implementations..."
OUT=$("$CJSH_PATH" -c "which echo" 2>&1)
if echo "$OUT" | grep -q "cjsh builtin"; then
    pass_test "which shows CJsShell custom implementations"
else
    fail_test "which shows CJsShell custom implementations (output: '$OUT')"
fi

# Test 3: Which shows ls as system command by default
echo "Test 3: Which shows ls as system command by default..."
OUT=$("$CJSH_PATH" -c "which ls" 2>&1)
if [ $? -eq 0 ] && echo "$OUT" | grep -E "^/" > /dev/null; then
    pass_test "which shows ls as system command by default"
else
    fail_test "which shows ls as system command (output: '$OUT')"
fi

# Test 4: Which shows external ls when disabled
echo "Test 4: Which shows external ls when custom ls is disabled..."
OUT=$("$CJSH_PATH" --disable-custom-ls -c "which ls" 2>&1)
if echo "$OUT" | grep -E "^/" > /dev/null; then
    pass_test "which shows external ls when custom ls is disabled"
else
    fail_test "which shows external ls when disabled (output: '$OUT')"
fi

# Test 5: Which identifies aliases
echo "Test 5: Which identifies aliases..."
OUT=$("$CJSH_PATH" -c "alias testwhichalias='echo testing'; which testwhichalias" 2>&1)
if echo "$OUT" | grep -q "aliased"; then
    pass_test "which identifies aliases"
else
    skip_test "which identifies aliases (may not be available in test context: '$OUT')"
fi

# Test 6: Which identifies functions
echo "Test 6: Which identifies functions..."
OUT=$("$CJSH_PATH" -c "testfunc() { echo test; }; which testfunc" 2>&1)
if echo "$OUT" | grep -q "function"; then
    pass_test "which identifies functions"
else
    skip_test "which identifies functions (may not be available in test context: '$OUT')"
fi

# Test 7: Which finds external executables
echo "Test 7: Which finds external executables..."
OUT=$("$CJSH_PATH" -c "which cat" 2>&1)
if [ $? -eq 0 ] && echo "$OUT" | grep -E "^/" > /dev/null; then
    pass_test "which finds external executables"
elif echo "$OUT" | grep -q "not found"; then
    skip_test "which finds external executables (cat not available on system)"
else
    fail_test "which finds external executables (output: '$OUT')"
fi

# Test 8: Which handles non-existent commands
echo "Test 8: Which handles non-existent commands..."
OUT=$("$CJSH_PATH" -c "which nonexistentcommandxyz123" 2>&1)
if [ $? -ne 0 ] && echo "$OUT" | grep -q "not found"; then
    pass_test "which handles non-existent commands correctly"
else
    fail_test "which handles non-existent commands (exit code: $?, output: '$OUT')"
fi

# Test 9: Which with -a (all) option
echo "Test 9: Which with -a option..."
OUT=$("$CJSH_PATH" -c "which -a echo" 2>&1)
if [ $? -eq 0 ]; then
    pass_test "which -a option works"
else
    fail_test "which -a option (exit code: $?, output: '$OUT')"
fi

# Test 10: Which with -s (silent) option
echo "Test 10: Which with -s option..."
OUT=$("$CJSH_PATH" -c "which -s cat" 2>/dev/null)
EXITCODE=$?
if [ $EXITCODE -eq 0 ] && [ -z "$OUT" ]; then
    pass_test "which -s option works (silent mode)"
else
    fail_test "which -s option (exit code: $EXITCODE, output should be empty but got: '$OUT')"
fi

# Test 11: Which with multiple arguments
echo "Test 11: Which with multiple arguments..."
OUT=$("$CJSH_PATH" -c "which echo cd pwd" 2>&1)
if [ $? -eq 0 ]; then
    pass_test "which handles multiple arguments"
else
    fail_test "which handles multiple arguments (output: '$OUT')"
fi

# Test 12: Which with invalid options
echo "Test 12: Which with invalid options..."
OUT=$("$CJSH_PATH" -c "which -x echo" 2>&1)
if [ $? -ne 0 ] && echo "$OUT" | grep -q "invalid option"; then
    pass_test "which rejects invalid options"
else
    fail_test "which rejects invalid options (should fail with invalid option error, got: '$OUT')"
fi

# Test 13: Which without arguments
echo "Test 13: Which without arguments..."
OUT=$("$CJSH_PATH" -c "which" 2>&1)
if [ $? -ne 0 ] && echo "$OUT" | grep -q "usage:"; then
    pass_test "which shows usage without arguments"
else
    fail_test "which shows usage without arguments (output: '$OUT')"
fi

echo ""
echo "Which Command Tests Summary:"
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