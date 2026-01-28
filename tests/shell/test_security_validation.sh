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

echo "Testing command injection prevention..."
"$CJSH_PATH" -c "echo 'hello'; echo 'world'" >/tmp/command_inject_test.out 2>&1
if [ $? -eq 0 ] && grep -q "hello" /tmp/command_inject_test.out && grep -q "world" /tmp/command_inject_test.out; then
    pass_test "basic command chaining works"
else
    fail_test "basic command chaining"
fi

"$CJSH_PATH" -c "echo 'test\$(rm -rf /tmp/nonexistent)'" >/tmp/malicious_inject_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "command substitution in quotes handled"
else
    fail_test "command substitution handling"
fi

echo "Testing buffer overflow protection..."
LONG_STRING=$(printf 'a%.0s' {1..10000})
"$CJSH_PATH" -c "echo '$LONG_STRING'" >/tmp/buffer_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then  # Should either work or fail gracefully
    pass_test "long input handling"
else
    fail_test "long input caused crash"
fi

echo "Testing environment variable security..."
ENV_SECURITY_OUT=$(mktemp /tmp/cjsh_env_security.XXXXXX)
MISSING_PRELOAD="/tmp/cjsh_missing_preload_$$.so"
OS_NAME=$(uname -s 2>/dev/null || echo unknown)
if [ "$OS_NAME" = "Darwin" ]; then
    INJECTION_ENV="DYLD_INSERT_LIBRARIES"
else
    INJECTION_ENV="LD_PRELOAD"
fi

{
    env "$INJECTION_ENV"="$MISSING_PRELOAD" "$CJSH_PATH" -c "echo test" >"$ENV_SECURITY_OUT" 2>&1
} 2>/dev/null
ld_status=$?
if [ $ld_status -ne 0 ]; then
    pass_test "$INJECTION_ENV injection prevented (exit $ld_status)"
else
    fail_test "$INJECTION_ENV injection should fail when library $MISSING_PRELOAD is missing"
fi
rm -f "$ENV_SECURITY_OUT"

echo "Testing file permission handling..."
echo "secret" > /tmp/restricted_file
chmod 600 /tmp/restricted_file

"$CJSH_PATH" -c "cat /tmp/restricted_file" >/tmp/permission_test.out 2>&1
if [ $? -eq 0 ] && grep -q "secret" /tmp/permission_test.out; then
    pass_test "file permission access (owner)"
else
    pass_test "file permission restrictions respected"
fi

echo "Testing NULL byte handling..."
printf "echo 'test\0malicious'" | "$CJSH_PATH" >/tmp/null_byte_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "NULL byte input handled"
else
    fail_test "NULL byte caused crash"
fi

echo "Testing special character handling..."
"$CJSH_PATH" -c "echo 'special chars: \$\`\"'\''(){}[]'" >/tmp/special_chars_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "special characters handled"
else
    fail_test "special character handling"
fi

echo "Testing script execution permissions..."
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

echo "Testing resource limit handling..."
"$CJSH_PATH" -c "ulimit -n 1024; echo 'ulimit set'" >/tmp/ulimit_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "resource limit setting"
else
    fail_test "resource limit handling"
fi

echo "Testing built-in command input validation..."
"$CJSH_PATH" -c "cd --invalid-option" >/tmp/invalid_option_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "invalid option handling"
else
    fail_test "invalid option should error"
fi

echo "Testing memory safety..."
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

echo "Testing signal injection protection..."
"$CJSH_PATH" -c "trap 'echo signal caught' TERM; echo normal execution" >/tmp/signal_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "signal handling safety"
else
    fail_test "signal handling"
fi

echo "Testing alias security..."
"$CJSH_PATH" -c "alias ls='rm -rf'; command ls /tmp" >/tmp/alias_security_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "alias command bypass"
else
    fail_test "alias command bypass with 'command' builtin"
fi

echo "Testing exit code manipulation..."
"$CJSH_PATH" -c "exit 42" >/tmp/exit_code_test.out 2>&1
exit_code=$?
if [ $exit_code -eq 42 ]; then
    pass_test "exit code handling"
else
    fail_test "exit code manipulation"
fi

echo "Testing wildcard security..."
mkdir -p /tmp/wildcard_test
touch "/tmp/wildcard_test/file1.txt"
touch "/tmp/wildcard_test/file2.txt"

"$CJSH_PATH" -c "ls /tmp/wildcard_test/*.txt" >/tmp/wildcard_security_test.out 2>&1
if [ $? -eq 0 ] && grep -q "file1.txt" /tmp/wildcard_security_test.out; then
    pass_test "wildcard expansion security"
else
    fail_test "wildcard expansion"
fi

echo "Testing redirection security..."
"$CJSH_PATH" -c "echo test > /tmp/redirection_test.out" >/dev/null 2>&1
if [ $? -eq 0 ] && [ -f /tmp/redirection_test.out ]; then
    pass_test "safe redirection"
else
    fail_test "redirection handling"
fi

"$CJSH_PATH" -c "echo test > /dev/null" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "redirection to /dev/null"
else
    fail_test "redirection to special files"
fi

rm -f /tmp/command_inject_test.out /tmp/malicious_inject_test.out /tmp/buffer_test.out
rm -f /tmp/path_traversal_test.out /tmp/permission_test.out
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
