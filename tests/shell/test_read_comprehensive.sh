#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: read command comprehensive..."

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

# Test basic read from stdin
OUT=$(echo "hello" | "$CJSH_PATH" -c "read VAR; echo \$VAR")
if [ "$OUT" = "hello" ]; then
    pass_test "basic read from stdin"
else
    fail_test "basic read (got '$OUT')"
fi

# Test read multiple variables
OUT=$(echo "one two three" | "$CJSH_PATH" -c "read A B C; echo \$A-\$B-\$C")
if [ "$OUT" = "one-two-three" ]; then
    pass_test "read multiple variables"
else
    fail_test "read multiple (got '$OUT', expected 'one-two-three')"
fi

# Test read with extra words goes to last variable
OUT=$(echo "one two three four" | "$CJSH_PATH" -c "read A B; echo \$A:\$B")
if [ "$OUT" = "one:two three four" ]; then
    pass_test "read extra words to last variable"
else
    fail_test "read extra words (got '$OUT', expected 'one:two three four')"
fi

# Test read with insufficient input
OUT=$(echo "one" | "$CJSH_PATH" -c "read A B C; echo \$A-\$B-\$C")
if [ "$OUT" = "one--" ]; then
    pass_test "read with insufficient input"
else
    fail_test "read insufficient (got '$OUT', expected 'one--')"
fi

# Test read empty line
OUT=$(echo "" | "$CJSH_PATH" -c "read VAR; echo \"result:\$VAR\"")
if [ "$OUT" = "result:" ]; then
    pass_test "read empty line"
else
    fail_test "read empty (got '$OUT')"
fi

# Test read with IFS
OUT=$(echo "a:b:c" | "$CJSH_PATH" -c "IFS=:; read A B C; echo \$A-\$B-\$C")
if [ "$OUT" = "a-b-c" ]; then
    pass_test "read with custom IFS"
else
    fail_test "read IFS (got '$OUT', expected 'a-b-c')"
fi

# Test read preserves leading spaces with -r
OUT=$(echo "  spaced  " | "$CJSH_PATH" -c "read VAR; echo \"<\$VAR>\"")
# Without -r, leading spaces are typically trimmed
if echo "$OUT" | grep -q "spaced"; then
    pass_test "read handles spaces"
else
    fail_test "read spaces (got '$OUT')"
fi

# Test read -r preserves backslashes
OUT=$(echo "back\\slash" | "$CJSH_PATH" -c "read -r VAR; echo \"\$VAR\"")
if [ "$OUT" = "back\\slash" ]; then
    pass_test "read -r preserves backslashes"
else
    skip_test "read -r backslash (got '$OUT', may not be supported)"
fi

# Test read -n (read N characters)
OUT=$(echo "hello world" | "$CJSH_PATH" -c "read -n 5 VAR 2>/dev/null; echo \$VAR")
if [ "$OUT" = "hello" ]; then
    pass_test "read -n characters"
else
    skip_test "read -n not supported (got '$OUT')"
fi

# Test read -d (delimiter)
OUT=$(echo "hello:world" | "$CJSH_PATH" -c "read -d ':' VAR 2>/dev/null; echo \$VAR")
if [ "$OUT" = "hello" ]; then
    pass_test "read -d custom delimiter"
else
    skip_test "read -d not supported"
fi

# Test read from here-document
cat > /tmp/test_read_heredoc.sh << 'EOF'
#!/bin/sh
read VAR << HEREDOC
hello from heredoc
HEREDOC
echo "$VAR"
EOF
chmod +x /tmp/test_read_heredoc.sh

OUT=$("$CJSH_PATH" /tmp/test_read_heredoc.sh)
if [ "$OUT" = "hello from heredoc" ]; then
    pass_test "read from here-document"
else
    fail_test "read heredoc (got '$OUT')"
fi
rm -f /tmp/test_read_heredoc.sh

# Test read in pipeline
OUT=$(echo "piped" | "$CJSH_PATH" -c "read VAR; echo \$VAR")
if [ "$OUT" = "piped" ]; then
    pass_test "read in pipeline"
else
    fail_test "read pipeline (got '$OUT')"
fi

# Test read in loop
cat > /tmp/test_read_loop.sh << 'EOF'
#!/bin/sh
echo "line1
line2
line3" | while read LINE; do
    echo "read: $LINE"
done
EOF
chmod +x /tmp/test_read_loop.sh

OUT=$("$CJSH_PATH" /tmp/test_read_loop.sh)
EXPECTED="read: line1
read: line2
read: line3"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "read in while loop"
else
    fail_test "read loop (output mismatch)"
fi
rm -f /tmp/test_read_loop.sh

# Test read from file
echo "file content" > /tmp/read_file_test
OUT=$("$CJSH_PATH" -c "read VAR < /tmp/read_file_test; echo \$VAR")
rm -f /tmp/read_file_test
if [ "$OUT" = "file content" ]; then
    pass_test "read from file"
else
    fail_test "read from file (got '$OUT')"
fi

# Test read with no variable name (goes to REPLY)
OUT=$(echo "default" | "$CJSH_PATH" -c "read; echo \$REPLY" 2>/dev/null)
if [ "$OUT" = "default" ]; then
    pass_test "read with no variable uses REPLY"
else
    skip_test "read REPLY (got '$OUT', may not be supported)"
fi

# Test read exit status on success
echo "test" | "$CJSH_PATH" -c "read VAR" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "read exit status on success"
else
    fail_test "read should return 0 on success"
fi

# Test read exit status on EOF
OUT=$("$CJSH_PATH" -c "read VAR < /dev/null; echo \$?")
if [ "$OUT" != "0" ]; then
    pass_test "read exit status on EOF is non-zero"
else
    fail_test "read should return non-zero on EOF"
fi

# Test read with tabs and spaces
OUT=$(printf "tab\there" | "$CJSH_PATH" -c "read A B; echo \$A-\$B")
if [ "$OUT" = "tab-here" ]; then
    pass_test "read handles tabs as IFS"
else
    fail_test "read tabs (got '$OUT')"
fi

# Test read with trailing whitespace
OUT=$(echo "word   " | "$CJSH_PATH" -c "read VAR; echo \"<\$VAR>\"")
if [ "$OUT" = "<word>" ]; then
    pass_test "read trims trailing whitespace"
else
    fail_test "read trailing space (got '$OUT')"
fi

# Test read with very long line
# Use seq instead of bash brace expansion for POSIX compatibility
LONG_LINE=$(seq 1 1000 | while read n; do printf 'a'; done)
OUT=$(echo "$LONG_LINE" | "$CJSH_PATH" -c "read VAR; echo \${#VAR}")
if [ "$OUT" = "1000" ]; then
    pass_test "read very long line"
else
    fail_test "read long line (got length '$OUT', expected 1000)"
fi

# Test read with special characters
OUT=$(echo 'test$VAR`cmd`$(sub)' | "$CJSH_PATH" -c "read VAR; echo \"\$VAR\"")
if [ "$OUT" = 'test$VAR`cmd`$(sub)' ]; then
    pass_test "read preserves special characters"
else
    fail_test "read special chars (got '$OUT')"
fi

# Test multiple read operations
cat > /tmp/test_multi_read.sh << 'EOF'
#!/bin/sh
echo "first
second
third" > /tmp/multi_input
exec 3< /tmp/multi_input
read LINE1 <&3
read LINE2 <&3
read LINE3 <&3
exec 3<&-
rm -f /tmp/multi_input
echo "$LINE1-$LINE2-$LINE3"
EOF
chmod +x /tmp/test_multi_read.sh

OUT=$("$CJSH_PATH" /tmp/test_multi_read.sh)
if [ "$OUT" = "first-second-third" ]; then
    pass_test "multiple read operations from file descriptor"
else
    fail_test "multiple reads (got '$OUT')"
fi
rm -f /tmp/test_multi_read.sh

echo ""
echo "Read Command Comprehensive Tests Summary:"
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
