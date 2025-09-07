#!/usr/bin/env sh
# Test command line argument parsing and shell options
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: command line options..."

# Test -c option (execute command)
OUT=$("$CJSH_PATH" -c "echo test-command")
if [ "$OUT" != "test-command" ]; then
    echo "FAIL: -c option (got '$OUT')"
    exit 1
fi

# Test -v/--version option
OUT=$("$CJSH_PATH" -v 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "FAIL: -v option should output version"
    exit 1
fi

OUT=$("$CJSH_PATH" --version 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "FAIL: --version option should output version"
    exit 1
fi

# Test -h/--help option
OUT=$("$CJSH_PATH" -h 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "FAIL: -h option should output help"
    exit 1
fi

OUT=$("$CJSH_PATH" --help 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "FAIL: --help option should output help"
    exit 1
fi

# Test --no-colors option
OUT=$("$CJSH_PATH" --no-colors -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-colors option (got '$OUT')"
    exit 1
fi

# Test --no-plugins option
echo "Testing --no-plugins option..."
OUT=$("$CJSH_PATH" --no-plugins -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-plugins option (got '$OUT')"
    exit 1
fi

# Test --no-themes option
echo "Testing --no-themes option..."
OUT=$("$CJSH_PATH" --no-themes -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-themes option (got '$OUT')"
    exit 1
fi

# Test --no-ai option
echo "Testing --no-ai option..."
OUT=$("$CJSH_PATH" --no-ai -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-ai option (got '$OUT')"
    exit 1
fi

# Test --no-plugins option
OUT=$("$CJSH_PATH" --no-plugins -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-plugins option (got '$OUT')"
    exit 1
fi

# Test --no-themes option
OUT=$("$CJSH_PATH" --no-themes -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-themes option (got '$OUT')"
    exit 1
fi

# Test --no-ai option
OUT=$("$CJSH_PATH" --no-ai -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: --no-ai option (got '$OUT')"
    exit 1
fi

# Test multiple options together
OUT=$("$CJSH_PATH" --no-colors --no-plugins --no-themes -c "echo multi-test")
if [ "$OUT" != "multi-test" ]; then
    echo "FAIL: multiple options (got '$OUT')"
    exit 1
fi

# Test invalid option handling
"$CJSH_PATH" --invalid-option 2>/dev/null
if [ $? -eq 0 ]; then
    echo "FAIL: invalid option should return non-zero exit code"
    exit 1
fi

# Test startup test mode
"$CJSH_PATH" --startup-test 2>/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: --startup-test should complete successfully"
    exit 1
fi

echo "PASS"
exit 0
