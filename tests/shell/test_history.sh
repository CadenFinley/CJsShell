#!/usr/bin/env sh
# Test history management and command recall
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: history management..."

# Create temporary directory for history tests
TEST_DIR=$(mktemp -d)
TEMP_HISTORY="$TEST_DIR/test_history"

# Test history command exists
OUT=$("$CJSH_PATH" -c "history" 2>/dev/null)
# History might be empty in non-interactive mode, so just check it doesn't error
if [ $? -ne 0 ]; then
    echo "FAIL: history command should be available"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test history builtin help
OUT=$("$CJSH_PATH" -c "help history" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "WARNING: history help not available"
fi

# We can't easily test interactive history features in non-interactive mode,
# but we can test that the history system doesn't crash

# Test that multiple commands don't break history
"$CJSH_PATH" -c "echo cmd1; echo cmd2; echo cmd3; history" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: multiple commands with history check"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test history with complex commands
"$CJSH_PATH" -c "echo 'test with quotes'; history" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: history with quoted commands"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test history with pipes
"$CJSH_PATH" -c "echo test | cat; history" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: history with piped commands"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test history with redirections
"$CJSH_PATH" -c "echo test > /dev/null; history" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: history with redirected commands"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Cleanup
rm -rf "$TEST_DIR"

echo "PASS"
exit 0
