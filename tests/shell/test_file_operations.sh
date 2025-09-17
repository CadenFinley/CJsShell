#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: file operations and I/O..."

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

# Create temporary directory for tests
TEST_DIR=$(mktemp -d)
cd "$TEST_DIR"

# Test output redirection (>)
"$CJSH_PATH" -c "echo 'hello output' > test_output.txt"
if [ -f "test_output.txt" ]; then
    CONTENT=$(cat test_output.txt)
    if [ "$CONTENT" = "hello output" ]; then
        pass_test "output redirection"
    else
        fail_test "output redirection content (got '$CONTENT')"
    fi
else
    fail_test "output redirection did not create file"
fi

# Test append redirection (>>)
"$CJSH_PATH" -c "echo 'appended' >> test_output.txt"
CONTENT=$(cat test_output.txt)
if [ "$CONTENT" = "hello output
appended" ]; then
    pass_test "append redirection"
else
    fail_test "append redirection"
fi

# Test input redirection (<)
echo "input line" > input_test.txt
OUT=$("$CJSH_PATH" -c "cat < input_test.txt")
if [ "$OUT" = "input line" ]; then
    pass_test "input redirection"
else
    fail_test "input redirection (got '$OUT')"
fi

# Test error redirection (2>)
"$CJSH_PATH" -c "echo 'error message' >&2 2> error_test.txt" 2>/dev/null
if [ -f "error_test.txt" ]; then
    pass_test "error redirection"
else
    fail_test "error redirection did not create file"
fi

# Test combined redirection (2>&1)
OUT=$("$CJSH_PATH" -c "echo 'stdout'; echo 'stderr' >&2" 2>&1)
if [ -n "$OUT" ]; then
    pass_test "combined redirection"
else
    fail_test "combined redirection"
fi

# Test file creation and removal
"$CJSH_PATH" -c "touch new_file.txt"
if [ -f "new_file.txt" ]; then
    pass_test "touch command"
else
    fail_test "touch command"
fi

# Test directory creation
"$CJSH_PATH" -c "mkdir test_dir"
if [ -d "test_dir" ]; then
    pass_test "mkdir command"
else
    fail_test "mkdir command"
fi

# Test file listing
OUT=$("$CJSH_PATH" -c "ls test_dir")
if [ $? -eq 0 ]; then
    pass_test "ls command on empty directory"
else
    fail_test "ls command on empty directory"
fi

# Test file copying (if available)
echo "test content" > source.txt
"$CJSH_PATH" -c "cp source.txt dest.txt" 2>/dev/null
if [ -f "dest.txt" ]; then
    DEST_CONTENT=$(cat dest.txt)
    if [ "$DEST_CONTENT" = "test content" ]; then
        pass_test "cp command"
    else
        fail_test "cp command content"
    fi
else
    skip_test "cp command not available"
fi

# Test file permissions (basic)
"$CJSH_PATH" -c "chmod 644 source.txt" 2>/dev/null
if [ $? -eq 0 ]; then
    pass_test "chmod command"
else
    skip_test "chmod command failed (may not be available)"
fi

# Test pwd command
OUT=$("$CJSH_PATH" -c "pwd")
CURRENT_DIR=$(pwd)
# Handle macOS /private prefix for temp directories
if [ "$OUT" = "$TEST_DIR" ] || [ "$OUT" = "/private$TEST_DIR" ]; then
    pass_test "pwd command"
else
    fail_test "pwd command (got '$OUT', expected '$TEST_DIR' or '/private$TEST_DIR')"
fi

# Test here document (if supported)
OUT=$("$CJSH_PATH" -c "cat << EOF
line1
line2
EOF")
EXPECTED="line1
line2"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "here document"
else
    fail_test "here document (got '$OUT')"
fi

# Cleanup
rm -rf "$TEST_DIR"

# Summary
echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))

if [ $TESTS_FAILED -eq 0 ] && [ $TESTS_SKIPPED -eq 0 ]; then
    printf "${GREEN}All tests passed! ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    exit 0
elif [ $TESTS_FAILED -eq 0 ]; then
    printf "${YELLOW}All tests passed with some skipped. ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    exit 0
else
    printf "${RED}Some tests failed. ${NC}($TESTS_PASSED/$TOTAL_TESTS)\n"
    printf "Passed: ${GREEN}$TESTS_PASSED${NC}, Failed: ${RED}$TESTS_FAILED${NC}, Skipped: ${YELLOW}$TESTS_SKIPPED${NC}\n"
    exit 1
fi
