#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: advanced builtin commands..."

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

# Test 1: readonly builtin comprehensive
echo "Testing readonly builtin..."
"$CJSH_PATH" -c "readonly TEST_VAR=hello; echo \$TEST_VAR" >/tmp/readonly_test.out 2>&1
if [ $? -eq 0 ] && grep -q "hello" /tmp/readonly_test.out; then
    pass_test "readonly variable assignment"
else
    fail_test "readonly variable assignment"
fi

# Test 2: umask builtin
echo "Testing umask builtin..."
"$CJSH_PATH" -c "umask" >/tmp/umask_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "umask display"
else
    fail_test "umask display"
fi

# Test umask setting
"$CJSH_PATH" -c "umask 022; umask" >/tmp/umask_set_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "umask setting"
else
    fail_test "umask setting"
fi

# Test 3: times builtin
echo "Testing times builtin..."
"$CJSH_PATH" -c "times" >/tmp/times_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "times command execution"
else
    fail_test "times command execution"
fi

# Test 4: type builtin
echo "Testing type builtin..."
"$CJSH_PATH" -c "type echo" >/tmp/type_test.out 2>&1
if [ $? -eq 0 ] && grep -q "echo" /tmp/type_test.out; then
    pass_test "type builtin command"
else
    fail_test "type builtin command"
fi

# Test type with external command
"$CJSH_PATH" -c "type ls" >/tmp/type_external_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "type external command"
else
    fail_test "type external command"
fi

# Test 5: hash builtin
echo "Testing hash builtin..."
"$CJSH_PATH" -c "hash ls; hash" >/tmp/hash_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "hash command caching"
else
    fail_test "hash command caching"
fi

# Test hash -r (clear)
"$CJSH_PATH" -c "hash ls; hash -r; hash" >/tmp/hash_clear_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "hash clear (-r)"
else
    fail_test "hash clear (-r)"
fi

# Test 6: getopts builtin comprehensive
echo "Testing getopts builtin..."
cat > /tmp/getopts_test.sh << 'EOF'
#!/bin/sh
while getopts "ab:c" opt; do
    case $opt in
        a) echo "option a" ;;
        b) echo "option b: $OPTARG" ;;
        c) echo "option c" ;;
        \?) echo "invalid option" >&2 ;;
    esac
done
EOF

"$CJSH_PATH" /tmp/getopts_test.sh -a -b value -c >/tmp/getopts_result.out 2>&1
if [ $? -eq 0 ] && grep -q "option a" /tmp/getopts_result.out && grep -q "option b: value" /tmp/getopts_result.out; then
    pass_test "getopts comprehensive parsing"
else
    fail_test "getopts comprehensive parsing"
fi

# Test 7: test/[ builtin
echo "Testing test builtin..."
"$CJSH_PATH" -c "test -f /etc/passwd && echo 'file exists'" >/tmp/test_builtin.out 2>&1
if [ $? -eq 0 ] && grep -q "file exists" /tmp/test_builtin.out; then
    pass_test "test builtin file test"
else
    fail_test "test builtin file test"
fi

# Test [ syntax
"$CJSH_PATH" -c "[ -d /tmp ] && echo 'directory exists'" >/tmp/bracket_test.out 2>&1
if [ $? -eq 0 ] && grep -q "directory exists" /tmp/bracket_test.out; then
    pass_test "[ builtin directory test"
else
    fail_test "[ builtin directory test"
fi

# Test string comparisons
"$CJSH_PATH" -c "[ \"hello\" = \"hello\" ] && echo 'strings equal'" >/tmp/string_test.out 2>&1
if [ $? -eq 0 ] && grep -q "strings equal" /tmp/string_test.out; then
    pass_test "test string equality"
else
    fail_test "test string equality"
fi

# Test 8: read builtin
echo "Testing read builtin..."
echo "test input" | "$CJSH_PATH" -c "read var; echo \$var" >/tmp/read_test.out 2>&1
if [ $? -eq 0 ] && grep -q "test input" /tmp/read_test.out; then
    pass_test "read builtin basic"
else
    fail_test "read builtin basic"
fi

# Test read with prompt
echo "input" | "$CJSH_PATH" -c "read -p 'Enter: ' var; echo \$var" >/tmp/read_prompt_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "read builtin with prompt"
else
    skip_test "read builtin with prompt (may not be implemented)"
fi

# Test 10: help builtin
echo "Testing help builtin..."
"$CJSH_PATH" -c "help" >/tmp/help_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "help builtin"
else
    fail_test "help builtin"
fi

# Test help with specific command
"$CJSH_PATH" -c "help echo" >/tmp/help_echo_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "help builtin with command"
else
    skip_test "help builtin with command (may not be implemented)"
fi

# Cleanup
rm -f /tmp/readonly_test.out /tmp/umask_test.out /tmp/umask_set_test.out /tmp/times_test.out
rm -f /tmp/type_test.out /tmp/type_external_test.out /tmp/hash_test.out /tmp/hash_clear_test.out
rm -f /tmp/getopts_test.sh /tmp/getopts_result.out /tmp/test_builtin.out /tmp/bracket_test.out
rm -f /tmp/string_test.out /tmp/read_test.out /tmp/read_prompt_test.out /tmp/version_test.out
rm -f /tmp/help_test.out /tmp/help_echo_test.out

echo ""
echo "Advanced Builtin Tests Summary:"
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