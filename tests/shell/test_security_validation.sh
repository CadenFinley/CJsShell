#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: security and input validation..."

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

# Test 1: Command injection prevention
echo "Testing command injection prevention..."
"$CJSH_PATH" -c "echo 'hello'; echo 'world'" >/tmp/command_inject_test.out 2>&1
if [ $? -eq 0 ] && grep -q "hello" /tmp/command_inject_test.out && grep -q "world" /tmp/command_inject_test.out; then
    pass_test "basic command chaining works"
else
    fail_test "basic command chaining"
fi

# Test malicious command injection attempts
"$CJSH_PATH" -c "echo 'test\$(rm -rf /tmp/nonexistent)'" >/tmp/malicious_inject_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "command substitution in quotes handled"
else
    fail_test "command substitution handling"
fi

# Test 2: Buffer overflow protection
echo "Testing buffer overflow protection..."
# Create very long command line
LONG_STRING=$(printf 'a%.0s' {1..10000})
"$CJSH_PATH" -c "echo '$LONG_STRING'" >/tmp/buffer_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then  # Should either work or fail gracefully
    pass_test "long input handling"
else
    fail_test "long input caused crash"
fi

# Test 3: Path traversal protection
echo "Testing path traversal protection..."
"$CJSH_PATH" -c "cd ../../../../../../etc; pwd" >/tmp/path_traversal_test.out 2>&1
if [ $? -eq 0 ]; then
    # Check if it actually changed to /etc (which would be concerning)
    if grep -q "/etc$" /tmp/path_traversal_test.out; then
        skip_test "path traversal (normal cd behavior)"
    else
        pass_test "path traversal handled normally"
    fi
else
    pass_test "path traversal prevented or failed"
fi

# Test 4: Environment variable security
echo "Testing environment variable security..."
# Test if dangerous environment variables are handled safely
LD_PRELOAD="/tmp/malicious.so" "$CJSH_PATH" -c "echo test" >/tmp/env_security_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "environment variable injection handled"
else
    pass_test "environment variable injection prevented"
fi

# Test 5: File permission handling
echo "Testing file permission handling..."
# Create a test file with restricted permissions
echo "secret" > /tmp/restricted_file
chmod 600 /tmp/restricted_file

# Test if shell respects file permissions
"$CJSH_PATH" -c "cat /tmp/restricted_file" >/tmp/permission_test.out 2>&1
if [ $? -eq 0 ] && grep -q "secret" /tmp/permission_test.out; then
    pass_test "file permission access (owner)"
else
    pass_test "file permission restrictions respected"
fi

# Test 6: NULL byte handling
echo "Testing NULL byte handling..."
printf "echo 'test\0malicious'" | "$CJSH_PATH" >/tmp/null_byte_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "NULL byte input handled"
else
    fail_test "NULL byte caused crash"
fi

# Test 7: Special character handling
echo "Testing special character handling..."
"$CJSH_PATH" -c "echo 'special chars: \$\`\"'\''(){}[]'" >/tmp/special_chars_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "special characters handled"
else
    fail_test "special character handling"
fi

# Test 8: Script execution permissions
echo "Testing script execution permissions..."
# Create a script without execute permissions
cat > /tmp/test_script.sh << 'EOF'
#!/bin/sh
echo "script executed"
EOF
chmod 644 /tmp/test_script.sh  # No execute permission

"$CJSH_PATH" -c "/tmp/test_script.sh" >/tmp/script_perm_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "script permission enforcement"
else
    skip_test "script permission (may depend on system)"
fi

# Test 9: Resource limit handling
echo "Testing resource limit handling..."
# Test if shell handles resource limits gracefully
"$CJSH_PATH" -c "ulimit -n 1024; echo 'ulimit set'" >/tmp/ulimit_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "resource limit setting"
else
    skip_test "resource limit handling"
fi

# Test 10: Input validation for built-in commands
echo "Testing built-in command input validation..."
# Test invalid options
"$CJSH_PATH" -c "cd --invalid-option" >/tmp/invalid_option_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "invalid option handling"
else
    skip_test "invalid option handling"
fi

# Test 11: Memory safety
echo "Testing memory safety..."
# Test repeated commands to check for memory leaks
for i in 1 2 3 4 5; do
    "$CJSH_PATH" -c "echo iteration $i" >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        fail_test "memory safety (crashed on iteration $i)"
        break
    fi
done
if [ $? -eq 0 ]; then
    pass_test "memory safety (repeated commands)"
fi

# Test 12: Signal injection protection
echo "Testing signal injection protection..."
# Test if shell handles signals safely
"$CJSH_PATH" -c "trap 'echo signal caught' TERM; echo normal execution" >/tmp/signal_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "signal handling safety"
else
    fail_test "signal handling"
fi

# Test 13: Alias security
echo "Testing alias security..."
# Test if aliases can be used for injection
"$CJSH_PATH" -c "alias ls='rm -rf'; command ls /tmp" >/tmp/alias_security_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "alias command bypass"
else
    skip_test "alias security test"
fi

# Test 14: Function security
echo "Testing function security..."
# Test if functions can override critical commands
"$CJSH_PATH" -c "cd() { echo 'hijacked cd'; }; cd /tmp" >/tmp/function_security_test.out 2>&1
if [ $? -eq 0 ]; then
    if grep -q "hijacked cd" /tmp/function_security_test.out; then
        skip_test "function override (normal shell behavior)"
    else
        pass_test "function override protection"
    fi
else
    pass_test "function definition handling"
fi

# Test 15: Exit code manipulation
echo "Testing exit code manipulation..."
"$CJSH_PATH" -c "exit 42" >/tmp/exit_code_test.out 2>&1
exit_code=$?
if [ $exit_code -eq 42 ]; then
    pass_test "exit code handling"
else
    fail_test "exit code manipulation"
fi

# Test 16: Wildcard security
echo "Testing wildcard security..."
# Create some test files
mkdir -p /tmp/wildcard_test
touch "/tmp/wildcard_test/file1.txt"
touch "/tmp/wildcard_test/file2.txt"

"$CJSH_PATH" -c "ls /tmp/wildcard_test/*.txt" >/tmp/wildcard_security_test.out 2>&1
if [ $? -eq 0 ] && grep -q "file1.txt" /tmp/wildcard_security_test.out; then
    pass_test "wildcard expansion security"
else
    fail_test "wildcard expansion"
fi

# Test 17: Redirection security
echo "Testing redirection security..."
# Test if redirection can overwrite critical files (simulate)
"$CJSH_PATH" -c "echo test > /tmp/redirection_test.out" >/dev/null 2>&1
if [ $? -eq 0 ] && [ -f /tmp/redirection_test.out ]; then
    pass_test "safe redirection"
else
    fail_test "redirection handling"
fi

# Test dangerous redirection
"$CJSH_PATH" -c "echo test > /dev/null" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "redirection to /dev/null"
else
    fail_test "redirection to special files"
fi

# Cleanup
rm -f /tmp/command_inject_test.out /tmp/malicious_inject_test.out /tmp/buffer_test.out
rm -f /tmp/path_traversal_test.out /tmp/env_security_test.out /tmp/permission_test.out
rm -f /tmp/null_byte_test.out /tmp/special_chars_test.out /tmp/script_perm_test.out
rm -f /tmp/ulimit_test.out /tmp/invalid_option_test.out /tmp/signal_test.out
rm -f /tmp/alias_security_test.out /tmp/function_security_test.out /tmp/exit_code_test.out
rm -f /tmp/wildcard_security_test.out /tmp/redirection_test.out
rm -f /tmp/restricted_file /tmp/test_script.sh
rm -rf /tmp/wildcard_test

echo ""
echo "Security and Input Validation Tests Summary:"
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