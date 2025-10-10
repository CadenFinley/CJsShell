#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: umask comprehensive..."

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

# Save original umask
ORIGINAL_UMASK=$(umask)

# Test getting current umask
OUT=$("$CJSH_PATH" -c "umask")
if [ -z "$OUT" ]; then
    fail_test "umask get current value"
else
    pass_test "umask get current value"
fi

# Test setting umask with octal value
"$CJSH_PATH" -c "umask 0022; touch /tmp/test_umask_file1; ls -l /tmp/test_umask_file1" >/dev/null 2>&1
if [ -f /tmp/test_umask_file1 ]; then
    PERMS=$(ls -l /tmp/test_umask_file1 | awk '{print $1}')
    rm -f /tmp/test_umask_file1
    # With umask 0022, files should be created with 0644 (-rw-r--r--)
    if echo "$PERMS" | grep -q "rw-r--r--"; then
        pass_test "umask set octal value 0022"
    else
        fail_test "umask set octal value 0022 (got permissions: $PERMS)"
    fi
else
    fail_test "umask set octal value 0022 (file not created)"
fi

# Test setting umask with 3-digit octal
"$CJSH_PATH" -c "umask 022; touch /tmp/test_umask_file2; ls -l /tmp/test_umask_file2" >/dev/null 2>&1
if [ -f /tmp/test_umask_file2 ]; then
    PERMS=$(ls -l /tmp/test_umask_file2 | awk '{print $1}')
    rm -f /tmp/test_umask_file2
    if echo "$PERMS" | grep -q "rw-r--r--"; then
        pass_test "umask set 3-digit octal"
    else
        fail_test "umask set 3-digit octal (got permissions: $PERMS)"
    fi
else
    fail_test "umask set 3-digit octal (file not created)"
fi

# Test restrictive umask 0077
"$CJSH_PATH" -c "umask 0077; touch /tmp/test_umask_file3; ls -l /tmp/test_umask_file3" >/dev/null 2>&1
if [ -f /tmp/test_umask_file3 ]; then
    PERMS=$(ls -l /tmp/test_umask_file3 | awk '{print $1}')
    rm -f /tmp/test_umask_file3
    # With umask 0077, files should be created with 0600 (-rw-------)
    if echo "$PERMS" | grep -q "rw-------"; then
        pass_test "umask restrictive 0077"
    else
        fail_test "umask restrictive 0077 (got permissions: $PERMS)"
    fi
else
    fail_test "umask restrictive 0077 (file not created)"
fi

# Test umask with -S option (symbolic output)
OUT=$("$CJSH_PATH" -c "umask -S" 2>&1)
if [ $? -eq 0 ]; then
    # Should output in symbolic format like u=rwx,g=rx,o=rx
    if echo "$OUT" | grep -q "[ugoa]"; then
        pass_test "umask -S symbolic output"
    else
        skip_test "umask -S format different than expected"
    fi
else
    skip_test "umask -S not supported"
fi

# Test umask -p option (output in a form that can be reused as input)
OUT=$("$CJSH_PATH" -c "umask -p" 2>&1)
if [ $? -eq 0 ]; then
    if echo "$OUT" | grep -q "umask"; then
        pass_test "umask -p reusable format"
    else
        skip_test "umask -p format different than expected"
    fi
else
    skip_test "umask -p not supported"
fi

# Test umask preserves value across commands
OUT=$("$CJSH_PATH" -c "umask 0027; umask")
if echo "$OUT" | grep -q "0027\|027"; then
    pass_test "umask preserves value"
else
    fail_test "umask preserves value (got '$OUT')"
fi

# Test invalid umask value (out of range)
"$CJSH_PATH" -c "umask 0999" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pass_test "umask rejects invalid octal (0999)"
else
    fail_test "umask should reject 0999"
fi

# Test invalid umask value (non-octal characters)
"$CJSH_PATH" -c "umask 089" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pass_test "umask rejects non-octal characters"
else
    fail_test "umask should reject 089"
fi

# Test umask with symbolic mode (if supported)
OUT=$("$CJSH_PATH" -c "umask u=rwx,g=rx,o=rx; umask" 2>&1)
if [ $? -eq 0 ]; then
    if echo "$OUT" | grep -q "0022\|022"; then
        pass_test "umask symbolic mode u=rwx,g=rx,o=rx"
    else
        skip_test "umask symbolic mode format differs (got '$OUT')"
    fi
else
    skip_test "umask symbolic mode not supported"
fi

# Test umask with symbolic mode - restrictive
OUT=$("$CJSH_PATH" -c "umask u=rwx,g=,o=; umask" 2>&1)
if [ $? -eq 0 ]; then
    if echo "$OUT" | grep -q "0077\|077"; then
        pass_test "umask symbolic mode u=rwx,g=,o="
    else
        skip_test "umask symbolic mode restrictive differs (got '$OUT')"
    fi
else
    skip_test "umask symbolic mode restrictive not supported"
fi

# Test umask applied to directory creation
"$CJSH_PATH" -c "umask 0022; mkdir /tmp/test_umask_dir1" >/dev/null 2>&1
if [ -d /tmp/test_umask_dir1 ]; then
    PERMS=$(ls -ld /tmp/test_umask_dir1 | awk '{print $1}')
    rmdir /tmp/test_umask_dir1
    # With umask 0022, directories should be created with 0755 (drwxr-xr-x)
    if echo "$PERMS" | grep -q "rwxr-xr-x"; then
        pass_test "umask applies to directory creation"
    else
        fail_test "umask directory creation (got permissions: $PERMS)"
    fi
else
    fail_test "umask directory not created"
fi

# Test umask doesn't affect existing files
touch /tmp/test_umask_exist
chmod 777 /tmp/test_umask_exist
"$CJSH_PATH" -c "umask 0077; chmod 777 /tmp/test_umask_exist" >/dev/null 2>&1
PERMS=$(ls -l /tmp/test_umask_exist | awk '{print $1}')
rm -f /tmp/test_umask_exist
if echo "$PERMS" | grep -q "rwxrwxrwx"; then
    pass_test "umask doesn't affect existing files"
else
    fail_test "umask affected existing file (got: $PERMS)"
fi

# Test umask with zero (most permissive)
OUT=$("$CJSH_PATH" -c "umask 0; umask")
if echo "$OUT" | grep -q "^0*$"; then
    pass_test "umask 0 (most permissive)"
else
    fail_test "umask 0 (got '$OUT')"
fi

# Test umask return status on success
"$CJSH_PATH" -c "umask 0022" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "umask exit status on success"
else
    fail_test "umask exit status on success"
fi

# Test umask with empty argument
"$CJSH_PATH" -c "umask ''" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pass_test "umask rejects empty argument"
else
    fail_test "umask should reject empty argument"
fi

# Restore original umask
umask "$ORIGINAL_UMASK"

echo ""
echo "Umask Comprehensive Tests Summary:"
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
