#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: syntax command builtin..."

TEST_DIR="/tmp/cjsh_syntax_tests_$$"
mkdir -p "$TEST_DIR"

OUT=$("$CJSH_PATH" -c "syntax" 2>&1)
if ! echo "$OUT" | grep -q "Usage:"; then
    echo "FAIL: syntax command without args should show usage (got '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
else
    echo "PASS: syntax command without args shows usage"
fi

cat > "$TEST_DIR/good_syntax.sh" << 'EOF'
#!/bin/bash
if [ "$USER" = "test" ]; then
    echo "Hello $USER"
fi

while [ $count -lt 10 ]; do
    echo "Count: $count"
    count=$((count + 1))
done

case $1 in
    "start")
        echo "Starting"
        ;;
    "stop")
        echo "Stopping"
        ;;
    *)
        echo "Unknown command"
        ;;
esac

function test_func() {
    echo "inside function"
}

echo "All done"
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/good_syntax.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax check on good file should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
else
    echo "PASS: syntax check on good file"
fi

cat > "$TEST_DIR/bad_syntax.sh" << 'EOF'
#!/bin/bash
echo "hello world

if [ "$USER" = "test" ]; then
    echo "Hello"

while true; do
    echo "running"

function test_func() {
    echo "inside function"
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/bad_syntax.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -Ei "error|critical"; then
    echo "FAIL: syntax check on bad file should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
else
    echo "PASS: syntax check on bad file"
fi

OUT=$("$CJSH_PATH" -c "syntax -c 'echo hello'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax -c with good command should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
else
    echo "PASS: syntax -c with good command"
fi

OUT=$("$CJSH_PATH" -c "syntax -c 'if [ true; then'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -Ei "critical|error"; then
    echo "FAIL: syntax -c with bad command should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "syntax -c 'echo hello world && ls'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax -c with complex good command should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "syntax /nonexistent/file.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "cannot open file"; then
    echo "FAIL: syntax on non-existent file should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

touch "$TEST_DIR/empty.sh"
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/empty.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax check on empty file should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

echo "#!/bin/bash" > "$TEST_DIR/shebang_only.sh"
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/shebang_only.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax check on shebang-only file should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

cat > "$TEST_DIR/complex_errors.sh" << 'EOF'
#!/bin/bash

case $1 in
    "start")
        echo "Starting"
        ;;

echo $(date

for i in 1 2 3; do
    echo $i

function bad_func() {
    if [ true; then
        echo "test"
}
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/complex_errors.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -Ei "error|critical"; then
    echo "FAIL: syntax check on complex errors should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

if ! echo "$OUT" | grep -q "(line "; then
    echo "FAIL: syntax errors should include line numbers (output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c 'echo "unclosed quote' 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    echo "FAIL: unclosed quote should return non-zero exit code (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi
if ! echo "$OUT" | grep -q "Unclosed quote"; then
    echo "FAIL: unclosed quote should report proper error (output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo 'unclosed single quote" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    echo "FAIL: unclosed single quote should return non-zero exit code (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi
if ! echo "$OUT" | grep -q "Unclosed quote"; then
    echo "FAIL: unclosed single quote should report proper error (output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

rm -rf "$TEST_DIR"

echo "PASS: syntax command without args shows usage"
echo "PASS: syntax check on good file"
echo "PASS: syntax check on bad file"
echo "PASS: syntax -c with good command"
echo "PASS: syntax -c with bad command"
echo "PASS: syntax -c with complex good command"
echo "PASS: syntax on non-existent file"
echo "PASS: syntax check on empty file"
echo "PASS: syntax check on shebang-only file"
echo "PASS: syntax check on complex errors"
echo "PASS: handling of unclosed double quotes"
echo "PASS: handling of unclosed single quotes"
exit 0
