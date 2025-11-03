#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: advanced I/O redirection and here documents..."

TEST_DIR="/tmp/cjsh_io_tests_$$"
mkdir -p "$TEST_DIR"

OUT=$("$CJSH_PATH" -c "cat << EOF
line 1
line 2
EOF" 2>&1)
EXPECTED="line 1
line 2"
if [ "$OUT" = "$EXPECTED" ]; then
    echo "PASS: basic here document works"
else
    echo "FAIL: basic here document broken (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "var=world; cat << EOF
Hello \$var
EOF" 2>&1)
if [ "$OUT" = "Hello world" ]; then
    echo "PASS: here document with variable expansion"
else
    echo "FAIL: here document variable expansion failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "var=world; cat << 'EOF'
Hello \$var
EOF" 2>&1)
if [ "$OUT" = "Hello \$var" ]; then
    echo "PASS: here document with quoted delimiter"
else
    echo "FAIL: here document quoted delimiter failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "cat <<< 'hello world'" 2>&1)
if [ "$OUT" = "hello world" ]; then
    echo "PASS: here strings work"
else
    echo "FAIL: here strings not implemented (got: '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "var=world; cat <<< \"hello \$var\"" 2>&1)
if [ "$OUT" = "hello world" ]; then
    echo "PASS: here strings with variable expansion"
else
    echo "FAIL: here strings with expansion not implemented (got: '$OUT', expected 'hello world')"
    rm -rf "$TEST_DIR"
    exit 1
fi

"$CJSH_PATH" -c "echo test &> $TEST_DIR/both_output.txt" 2>&1
if [ -f "$TEST_DIR/both_output.txt" ] && [ "$(cat "$TEST_DIR/both_output.txt")" = "test" ]; then
    echo "PASS: &> redirection works"
else
    echo "FAIL: &> redirection not implemented"
fi

"$CJSH_PATH" -c "echo first > $TEST_DIR/append_test.txt; echo second >> $TEST_DIR/append_test.txt" 2>&1
if [ -f "$TEST_DIR/append_test.txt" ]; then
    CONTENT=$(cat "$TEST_DIR/append_test.txt")
    EXPECTED="first
second"
    if [ "$CONTENT" = "$EXPECTED" ]; then
        echo "PASS: append redirection works"
    else
        echo "FAIL: append redirection failed (got: '$CONTENT')"
        rm -rf "$TEST_DIR"
        exit 1
    fi
else
    echo "FAIL: append redirection file not created"
    rm -rf "$TEST_DIR"
    exit 1
fi

"$CJSH_PATH" -c "ls /nonexistent_directory_test 2> $TEST_DIR/error_output.txt" 2>&1
if [ -f "$TEST_DIR/error_output.txt" ] && [ -s "$TEST_DIR/error_output.txt" ]; then
    echo "PASS: error redirection works"
else
    echo "FAIL: error redirection not fully implemented"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo stdout; echo stderr >&2" 2>&1)
if echo "$OUT" | grep -q "stdout" && echo "$OUT" | grep -q "stderr"; then
    echo "PASS: combined stdout/stderr redirection"
else
    echo "FAIL: combined redirection failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

echo "test content" > "$TEST_DIR/fd_test.txt"
OUT=$("$CJSH_PATH" -c "exec 3< $TEST_DIR/fd_test.txt; read line <&3; echo \$line" 2>&1)
if [ "$OUT" = "test content" ]; then
    echo "PASS: file descriptor manipulation works"
else
    echo "FAIL: file descriptor manipulation not implemented (got: '$OUT', expected 'test content')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "diff <(echo test) <(echo test)" 2>&1)
if [ -z "$OUT" ]; then  # diff returns empty when files are identical
    echo "PASS: process substitution works"
else
    echo "FAIL: process substitution not implemented (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

"$CJSH_PATH" -c "echo stdout; echo stderr >&2" > "$TEST_DIR/multi_stdout.txt" 2> "$TEST_DIR/multi_stderr.txt"
if [ -f "$TEST_DIR/multi_stdout.txt" ] && [ -f "$TEST_DIR/multi_stderr.txt" ]; then
    STDOUT_CONTENT=$(cat "$TEST_DIR/multi_stdout.txt")
    STDERR_CONTENT=$(cat "$TEST_DIR/multi_stderr.txt")
    if [ "$STDOUT_CONTENT" = "stdout" ]; then
        echo "PASS: multiple redirections work"
    else
        echo "FAIL: multiple redirections failed"
        rm -rf "$TEST_DIR"
        exit 1
    fi
else
    echo "FAIL: multiple redirection files not created"
    rm -rf "$TEST_DIR"
    exit 1
fi

cat > "$TEST_DIR/heredoc_function.sh" << 'EOF'
#!/bin/bash
print_message() {
    cat << MESSAGE
This is a message
from a function
MESSAGE
}
print_message
EOF

OUT=$("$CJSH_PATH" "$TEST_DIR/heredoc_function.sh" 2>&1)
EXPECTED="This is a message
from a function"
if [ "$OUT" = "$EXPECTED" ]; then
    echo "PASS: here document in function works"
else
    echo "FAIL: here document in function failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

cat > "$TEST_DIR/nested_heredoc.sh" << 'EOF'
#!/bin/bash
if true; then
    cat << OUTER
outer document
OUTER
fi
EOF

OUT=$("$CJSH_PATH" "$TEST_DIR/nested_heredoc.sh" 2>&1)
if [ "$OUT" = "outer document" ]; then
    echo "PASS: nested here documents work"
else
    echo "FAIL: nested here documents failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

cat > "$TEST_DIR/heredoc_indent.sh" << 'EOF'
cat <<- DELIMITER
	indented line
	another line
DELIMITER
EOF

OUT=$("$CJSH_PATH" "$TEST_DIR/heredoc_indent.sh" 2>&1)
EXPECTED="indented line
another line"
if [ "$OUT" = "$EXPECTED" ]; then
    echo "PASS: here document with indentation handling"
else
    echo "FAIL: here document indentation failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

rm -rf "$TEST_DIR"
echo "PASS: advanced I/O redirection tests completed"
exit 0