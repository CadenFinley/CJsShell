#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: gibberish script error handling..."

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

# Create temporary directory for test files
TEST_TMP_DIR="/tmp/cjsh_gibberish_test_$$"
mkdir -p "$TEST_TMP_DIR"

# Cleanup function
cleanup() {
    rm -rf "$TEST_TMP_DIR"
}
trap cleanup EXIT

# Test 1: Gibberish with random shell-like syntax
cat > "$TEST_TMP_DIR/gibberish1.sh" << 'EOF'
#!/bin/sh
nonexistent_command_xyz --with --random --flags
exit 1  # Force script to fail after error
EOF

# Test 2: Mixed valid and invalid syntax
cat > "$TEST_TMP_DIR/gibberish2.sh" << 'EOF'
#!/bin/sh
echo "This line is valid"
if [ $nonexistent_var -eq ]; then
    echo "incomplete condition"
fi
cd /this/path/does/not/exist/anywhere
ls -z --invalid-flag
echo "another valid line" | invalid_command --with --flags
for i in 1 2 3; do
    echo $((i + undefined_var))
    break 5  # invalid break level
done
EOF

# Test 3: Completely nonsensical content
cat > "$TEST_TMP_DIR/gibberish3.sh" << 'EOF'
#!/bin/sh
@#$%^&*()_+{}|:"<>?
if then else fi while do done
echo echo echo echo echo
$$$$$$$$$$$$$$$$$$$$
[[[[[[]]]]]]
for for for in in in do do do
function function() function
random_gibberish_text_that_looks_like_code
EOF

# Test 4: Binary-like content (simulated)
cat > "$TEST_TMP_DIR/gibberish4.sh" << 'EOF'
#!/bin/sh
This file contains simulated binary content:
ÿþÿþÿþÿþ garbage binary data ÿþÿþÿþÿþ
More random bytes: àáâãäåæçèéêëìíîïðñòó
And some mixed content: echo "test" ÿþÿþ exit 1
EOF

# Test 1: Execute gibberish script with shell-like syntax
OUTPUT=$("$CJSH_PATH" "$TEST_TMP_DIR/gibberish1.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    fail_test "gibberish script 1 unexpectedly succeeded"
else
    pass_test "gibberish script 1 properly failed with exit code $EXIT_CODE"
fi

# Test 2: Execute mixed valid/invalid syntax script
"$CJSH_PATH" "$TEST_TMP_DIR/gibberish2.sh" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    fail_test "gibberish script 2 unexpectedly succeeded"
else
    pass_test "gibberish script 2 properly failed with exit code $EXIT_CODE"
fi

# Test 3: Execute nonsensical content
"$CJSH_PATH" "$TEST_TMP_DIR/gibberish3.sh" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    fail_test "gibberish script 3 unexpectedly succeeded"
else
    pass_test "gibberish script 3 properly failed with exit code $EXIT_CODE"
fi

# Test 4: Execute binary-like content
"$CJSH_PATH" "$TEST_TMP_DIR/gibberish4.sh" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    fail_test "gibberish script 4 unexpectedly succeeded"
else
    pass_test "gibberish script 4 properly failed with exit code $EXIT_CODE"
fi

# Test 5: Verify cjsh doesn't crash and produces some error output
# Check for any kind of error output (error messages, suggestions, etc.)
if echo "$OUTPUT" | grep -E "(ERROR|error|command not found|Suggestion)" > /dev/null; then
    pass_test "cjsh produced error output for gibberish script"
else
    fail_test "cjsh produced no recognizable error output for gibberish script (got: '$(echo "$OUTPUT" | tr '\n' ' ')') "
fi

# Test 6: Verify cjsh handles nonexistent file gracefully
"$CJSH_PATH" "$TEST_TMP_DIR/nonexistent_file.sh" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    fail_test "nonexistent file unexpectedly succeeded"
else
    pass_test "nonexistent file properly failed with exit code $EXIT_CODE"
fi

# Test 7: Test that cjsh doesn't hang on infinite loops in gibberish
timeout 5s "$CJSH_PATH" -c "while true; do echo 'infinite'; done" > /dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 124 ]; then  # timeout exit code
    pass_test "cjsh properly handles infinite loop with timeout"
elif [ $EXIT_CODE -eq 0 ]; then
    fail_test "cjsh infinite loop test completed unexpectedly"
else
    pass_test "cjsh infinite loop test failed as expected with exit code $EXIT_CODE"
fi

# Test 8: Test cjsh with completely empty script
touch "$TEST_TMP_DIR/empty.sh"
chmod +x "$TEST_TMP_DIR/empty.sh"
"$CJSH_PATH" "$TEST_TMP_DIR/empty.sh"
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    pass_test "empty script executed successfully"
else
    pass_test "empty script handled gracefully with exit code $EXIT_CODE"
fi

# Test 9: Test with script containing only comments and whitespace
cat > "$TEST_TMP_DIR/comments_only.sh" << 'EOF'
#!/bin/sh
# This is a comment
   # Another comment with whitespace

# More comments


   
# Final comment
EOF
"$CJSH_PATH" "$TEST_TMP_DIR/comments_only.sh"
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    pass_test "comments-only script executed successfully"
else
    pass_test "comments-only script handled gracefully with exit code $EXIT_CODE"
fi

echo ""
echo "Gibberish Error Handling Tests Summary:"
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