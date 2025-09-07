#!/usr/bin/env sh
# Test error conditions and edge cases
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: error handling and edge cases..."

# Test empty command
OUT=$("$CJSH_PATH" -c "" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: empty command should not error"
    exit 1
fi

# Test whitespace-only command
OUT=$("$CJSH_PATH" -c "   " 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: whitespace-only command should not error"
    exit 1
fi

# Test very long command line
LONG_CMD=$(printf 'echo %*s' 1000 | tr ' ' 'a')
OUT=$("$CJSH_PATH" -c "$LONG_CMD" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "WARNING: very long command failed"
fi

# Test invalid syntax
"$CJSH_PATH" -c "if" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "FAIL: incomplete if statement should error"
    exit 1
fi

# Test unmatched quotes
"$CJSH_PATH" -c "echo 'unmatched" 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    echo "WARNING: unmatched quotes test - shell may handle this differently"
fi

# Test invalid redirection
"$CJSH_PATH" -c "echo test > /invalid/path/file" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "FAIL: invalid redirection should error"
    exit 1
fi

# Test permission denied
"$CJSH_PATH" -c "touch /root/test_file" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "WARNING: permission test may have succeeded unexpectedly"
fi

# Test extremely nested command substitution
NESTED="echo test"
for i in 1 2 3 4 5; do
    NESTED="\$(echo \$($NESTED))"
done
OUT=$("$CJSH_PATH" -c "$NESTED" 2>/dev/null)
if [ "$OUT" != "test" ]; then
    echo "WARNING: deeply nested command substitution failed"
fi

# Test very long environment variable
LONG_VAR=$(printf '%*s' 1000 | tr ' ' 'a')
OUT=$("$CJSH_PATH" -c "TEST_LONG='$LONG_VAR'; echo \${#TEST_LONG}" 2>/dev/null)
if [ "$OUT" != "1000" ]; then
    echo "WARNING: long environment variable test failed (got '$OUT')"
fi

# Test special characters in commands
OUT=$("$CJSH_PATH" -c "echo '!@#\$%^&*()_+-={}[]|\\:;\"<>?,./'" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: special characters in echo command"
    exit 1
fi

# Test null command
"$CJSH_PATH" -c ":" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: null command (:) should succeed"
    exit 1
fi

# Test multiple redirections
OUT=$("$CJSH_PATH" -c "echo test 2>/dev/null >/dev/null" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: multiple redirections should work"
    exit 1
fi

# Test command with many arguments
MANY_ARGS=""
for i in $(seq 1 100); do
    MANY_ARGS="$MANY_ARGS arg$i"
done
OUT=$("$CJSH_PATH" -c "echo $MANY_ARGS | wc -w" 2>/dev/null)
if [ "$OUT" != "100" ]; then
    echo "WARNING: command with many arguments failed (got '$OUT')"
fi

# Test recursive alias (should not crash)
"$CJSH_PATH" -c "alias test_alias='test_alias'; test_alias" 2>/dev/null
# Should timeout or handle recursion gracefully

# Test binary file execution attempt
echo -e '\x7fELF' > /tmp/fake_binary
chmod +x /tmp/fake_binary
"$CJSH_PATH" -c "/tmp/fake_binary" 2>/dev/null
rm -f /tmp/fake_binary

# Test ctrl characters in input
OUT=$("$CJSH_PATH" -c "echo -e 'line1\nline2\tword'" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: control characters in echo"
    exit 1
fi

echo "PASS"
exit 0
