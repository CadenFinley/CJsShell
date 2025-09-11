#!/usr/bin/env sh
# Test syntax command builtin comprehensively
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: syntax command builtin..."

# Create temporary test files
TEST_DIR="/tmp/cjsh_syntax_tests_$$"
mkdir -p "$TEST_DIR"

# Test 1: syntax command without arguments (should show usage)
OUT=$("$CJSH_PATH" -c "syntax" 2>&1)
if ! echo "$OUT" | grep -q "Usage:"; then
    echo "FAIL: syntax command without args should show usage (got '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 2: Create a file with correct syntax
cat > "$TEST_DIR/good_syntax.sh" << 'EOF'
#!/bin/bash
if [ "$USER" = "test" ]; then
    echo "Hello $USER"
fi

while [ $count -lt 10 ]; do
    echo "Count: $count"
    count=$((count + 1))
done

case $1 in
    "start")
        echo "Starting"
        ;;
    "stop")
        echo "Stopping"
        ;;
    *)
        echo "Unknown command"
        ;;
esac

function test_func() {
    echo "inside function"
}

echo "All done"
EOF

# Test syntax check on good file
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/good_syntax.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax check on good file should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 3: Create a file with syntax errors
cat > "$TEST_DIR/bad_syntax.sh" << 'EOF'
#!/bin/bash
# Missing closing quote
echo "hello world

# Unmatched if
if [ "$USER" = "test" ]; then
    echo "Hello"
# Missing fi

# Unmatched while
while true; do
    echo "running"
# Missing done

# Function with missing brace
function test_func() {
    echo "inside function"
# Missing }
EOF

# Test syntax check on bad file
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/bad_syntax.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "Syntax errors found"; then
    echo "FAIL: syntax check on bad file should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 4: Test -c option with good command
OUT=$("$CJSH_PATH" -c "syntax -c 'echo hello'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax -c with good command should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 5: Test -c option with bad command
OUT=$("$CJSH_PATH" -c "syntax -c 'if [ true; then'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "Syntax errors found"; then
    echo "FAIL: syntax -c with bad command should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 6: Test -c option with multiple words
OUT=$("$CJSH_PATH" -c "syntax -c 'echo hello world && ls'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax -c with complex good command should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 7: Test non-existent file
OUT=$("$CJSH_PATH" -c "syntax /nonexistent/file.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "cannot open file"; then
    echo "FAIL: syntax on non-existent file should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 8: Test empty file
touch "$TEST_DIR/empty.sh"
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/empty.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax check on empty file should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 9: Test with shebang line only
echo "#!/bin/bash" > "$TEST_DIR/shebang_only.sh"
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/shebang_only.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax check on shebang-only file should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 10: Test complex syntax errors
cat > "$TEST_DIR/complex_errors.sh" << 'EOF'
#!/bin/bash
# Multiple syntax errors

# Unmatched case
case $1 in
    "start")
        echo "Starting"
        ;;
# Missing esac

# Unmatched parentheses in command substitution
echo $(date

# Incomplete for loop
for i in 1 2 3; do
    echo $i
# Missing done

# Function with syntax error
function bad_func() {
    if [ true; then
        echo "test"
    # Missing fi
}
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/complex_errors.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "Syntax errors found"; then
    echo "FAIL: syntax check on complex errors should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Verify that multiple errors are reported
if ! echo "$OUT" | grep -q "Line"; then
    echo "FAIL: syntax errors should include line numbers (output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Clean up
rm -rf "$TEST_DIR"

echo "PASS"
exit 0
