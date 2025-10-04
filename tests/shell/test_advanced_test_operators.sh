#!/usr/bin/env sh
# Test advanced test command operators that are currently missing in cjsh

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: Advanced test operators (POSIX compliance gaps)..."

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

# Create test files and links for testing
TEST_DIR="/tmp/cjsh_test_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR" || exit 1

# Create test files
touch regular_file
mkdir test_dir
ln -s regular_file symlink_file 2>/dev/null
mkfifo named_pipe 2>/dev/null

# Test 1: -L (symbolic link test)
echo "Test -L operator for symbolic links"
if [ -L symlink_file ]; then
    # Reference implementation works
    "$CJSH_PATH" -c "test -L $TEST_DIR/symlink_file" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -L (symbolic link detection)"
    else
        fail_test "test -L (symbolic link detection) - not implemented"
    fi
else
    skip_test "test -L (symbolic link) - system doesn't support symlinks"
fi

# Test 2: -S (socket test)
echo "Test -S operator for sockets"
# Create a socket using a helper if available
if command -v nc >/dev/null 2>&1 || command -v socat >/dev/null 2>&1; then
    skip_test "test -S (socket detection) - requires socket creation (complex setup)"
else
    skip_test "test -S (socket detection) - no socket creation tools available"
fi

# Test 3: -p (named pipe test)
echo "Test -p operator for named pipes"
if [ -p named_pipe ]; then
    # Reference implementation works
    "$CJSH_PATH" -c "test -p $TEST_DIR/named_pipe" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -p (named pipe detection)"
    else
        fail_test "test -p (named pipe detection) - not implemented"
    fi
else
    skip_test "test -p (named pipe) - system doesn't support named pipes"
fi

# Test 4: -b (block device test)
echo "Test -b operator for block devices"
BLOCK_DEV=""
if [ -b /dev/disk0 ]; then
    BLOCK_DEV="/dev/disk0"  # macOS
elif [ -b /dev/sda ]; then
    BLOCK_DEV="/dev/sda"  # Linux
fi

if [ -n "$BLOCK_DEV" ]; then
    "$CJSH_PATH" -c "test -b $BLOCK_DEV" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -b (block device detection)"
    else
        fail_test "test -b (block device detection) - not implemented"
    fi
else
    skip_test "test -b (block device) - no block device found"
fi

# Test 5: -c (character device test)
echo "Test -c operator for character devices"
if [ -c /dev/null ]; then
    "$CJSH_PATH" -c "test -c /dev/null" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -c (character device detection)"
    else
        fail_test "test -c (character device detection) - not implemented"
    fi
else
    skip_test "test -c (character device) - /dev/null not found"
fi

# Test 6: -u (setuid bit test)
echo "Test -u operator for setuid bit"
# Find a setuid file
SETUID_FILE=""
for file in /bin/su /usr/bin/su /bin/sudo /usr/bin/sudo; do
    if [ -u "$file" ] 2>/dev/null; then
        SETUID_FILE="$file"
        break
    fi
done

if [ -n "$SETUID_FILE" ]; then
    "$CJSH_PATH" -c "test -u $SETUID_FILE" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -u (setuid bit detection)"
    else
        fail_test "test -u (setuid bit detection) - not implemented"
    fi
else
    skip_test "test -u (setuid bit) - no setuid file found"
fi

# Test 7: -g (setgid bit test)
echo "Test -g operator for setgid bit"
# Create a test file with setgid bit
touch setgid_test
chmod g+s setgid_test 2>/dev/null

if [ -g setgid_test ]; then
    "$CJSH_PATH" -c "test -g $TEST_DIR/setgid_test" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -g (setgid bit detection)"
    else
        fail_test "test -g (setgid bit detection) - not implemented"
    fi
else
    skip_test "test -g (setgid bit) - cannot set setgid on test file"
fi

# Test 8: -k (sticky bit test)
echo "Test -k operator for sticky bit"
# Create a test directory with sticky bit
mkdir sticky_dir 2>/dev/null
chmod +t sticky_dir 2>/dev/null

if [ -k sticky_dir ]; then
    "$CJSH_PATH" -c "test -k $TEST_DIR/sticky_dir" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -k (sticky bit detection)"
    else
        fail_test "test -k (sticky bit detection) - not implemented"
    fi
else
    skip_test "test -k (sticky bit) - cannot set sticky bit on test directory"
fi

# Test 9: -nt (newer than test)
echo "Test -nt operator for file comparison"
touch older_file
sleep 1
touch newer_file

if [ newer_file -nt older_file ]; then
    # Reference implementation works
    "$CJSH_PATH" -c "test $TEST_DIR/newer_file -nt $TEST_DIR/older_file" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -nt (newer than comparison)"
    else
        fail_test "test -nt (newer than comparison) - not implemented"
    fi
else
    skip_test "test -nt - file timestamp comparison not working in reference shell"
fi

# Test 10: -ot (older than test)
echo "Test -ot operator for file comparison"
if [ older_file -ot newer_file ]; then
    # Reference implementation works
    "$CJSH_PATH" -c "test $TEST_DIR/older_file -ot $TEST_DIR/newer_file" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -ot (older than comparison)"
    else
        fail_test "test -ot (older than comparison) - not implemented"
    fi
else
    skip_test "test -ot - file timestamp comparison not working in reference shell"
fi

# Test 11: -ef (same file test)
echo "Test -ef operator for file equality"
ln regular_file hardlink_file 2>/dev/null

if [ -f hardlink_file ] && [ regular_file -ef hardlink_file ]; then
    # Reference implementation works
    "$CJSH_PATH" -c "test $TEST_DIR/regular_file -ef $TEST_DIR/hardlink_file" 2>/dev/null
    if [ $? -eq 0 ]; then
        pass_test "test -ef (same file comparison)"
    else
        fail_test "test -ef (same file comparison) - not implemented"
    fi
else
    skip_test "test -ef - hard links not supported or reference shell doesn't support -ef"
fi

# Cleanup
cd /
rm -rf "$TEST_DIR"

# Print summary
echo ""
echo "================================"
echo "Advanced Test Operators Summary:"
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "  SKIPPED: $TESTS_SKIPPED"
echo "================================"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi

exit 0
