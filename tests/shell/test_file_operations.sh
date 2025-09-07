#!/usr/bin/env sh
# Test file operations and I/O redirection
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: file operations and I/O..."

# Create temporary directory for tests
TEST_DIR=$(mktemp -d)
cd "$TEST_DIR"

# Test output redirection (>)
"$CJSH_PATH" -c "echo 'hello output' > test_output.txt"
if [ ! -f "test_output.txt" ]; then
    echo "FAIL: output redirection did not create file"
    rm -rf "$TEST_DIR"
    exit 1
fi

CONTENT=$(cat test_output.txt)
if [ "$CONTENT" != "hello output" ]; then
    echo "FAIL: output redirection content (got '$CONTENT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test append redirection (>>)
"$CJSH_PATH" -c "echo 'appended' >> test_output.txt"
CONTENT=$(cat test_output.txt)
if [ "$CONTENT" != "hello output
appended" ]; then
    echo "FAIL: append redirection"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test input redirection (<)
echo "input line" > input_test.txt
OUT=$("$CJSH_PATH" -c "cat < input_test.txt")
if [ "$OUT" != "input line" ]; then
    echo "FAIL: input redirection (got '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test error redirection (2>)
"$CJSH_PATH" -c "echo 'error message' >&2 2> error_test.txt" 2>/dev/null
if [ ! -f "error_test.txt" ]; then
    echo "FAIL: error redirection did not create file"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test combined redirection (2>&1)
OUT=$("$CJSH_PATH" -c "echo 'stdout'; echo 'stderr' >&2" 2>&1)
if [ -z "$OUT" ]; then
    echo "FAIL: combined redirection"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test file creation and removal
"$CJSH_PATH" -c "touch new_file.txt"
if [ ! -f "new_file.txt" ]; then
    echo "FAIL: touch command"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test directory creation
"$CJSH_PATH" -c "mkdir test_dir"
if [ ! -d "test_dir" ]; then
    echo "FAIL: mkdir command"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test file listing
OUT=$("$CJSH_PATH" -c "ls test_dir")
if [ $? -ne 0 ]; then
    echo "FAIL: ls command on empty directory"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test file copying (if available)
echo "test content" > source.txt
"$CJSH_PATH" -c "cp source.txt dest.txt" 2>/dev/null
if [ -f "dest.txt" ]; then
    DEST_CONTENT=$(cat dest.txt)
    if [ "$DEST_CONTENT" != "test content" ]; then
        echo "FAIL: cp command content"
        rm -rf "$TEST_DIR"
        exit 1
    fi
fi

# Test file permissions (basic)
"$CJSH_PATH" -c "chmod 644 source.txt" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "WARNING: chmod command failed (may not be available)"
fi

# Test pwd command
OUT=$("$CJSH_PATH" -c "pwd")
CURRENT_DIR=$(pwd)
# Handle macOS /private prefix for temp directories
if [ "$OUT" != "$TEST_DIR" ] && [ "$OUT" != "/private$TEST_DIR" ]; then
    echo "FAIL: pwd command (got '$OUT', expected '$TEST_DIR' or '/private$TEST_DIR')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test here document (if supported)
OUT=$("$CJSH_PATH" -c "cat << EOF
line1
line2
EOF")
EXPECTED="line1
line2"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: here document (got '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Cleanup
rm -rf "$TEST_DIR"

echo "PASS"
exit 0
