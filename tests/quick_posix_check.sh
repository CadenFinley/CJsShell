#!/usr/bin/env sh
# Quick POSIX Compliance Test
# Run this for a fast overview of POSIX compliance status

SHELL_TO_TEST="${1:-./build/cjsh}"

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "=== Quick POSIX Compliance Check ==="
echo "Shell: $SHELL_TO_TEST"
echo ""

# Quick tests for core functionality
echo "Core functionality:"

# Basic command execution
if $SHELL_TO_TEST -c "echo test" >/dev/null 2>&1; then
    echo "✅ Basic command execution"
else
    echo "❌ Basic command execution"
fi

# Variable expansion
if [ "$($SHELL_TO_TEST -c 'VAR=hello; echo $VAR' 2>/dev/null)" = "hello" ]; then
    echo "✅ Variable expansion"
else
    echo "❌ Variable expansion"
fi

# Pipelines
if [ "$($SHELL_TO_TEST -c 'echo hello | cat' 2>/dev/null)" = "hello" ]; then
    echo "✅ Pipelines"
else
    echo "❌ Pipelines"
fi

# Redirection
if $SHELL_TO_TEST -c 'echo test > /tmp/quick_test_$$; cat /tmp/quick_test_$$; rm -f /tmp/quick_test_$$' >/dev/null 2>&1; then
    echo "✅ I/O Redirection"
else
    echo "❌ I/O Redirection"
fi

# Command substitution
if [ "$($SHELL_TO_TEST -c 'echo $(echo nested)' 2>/dev/null)" = "nested" ]; then
    echo "✅ Command substitution"
else
    echo "❌ Command substitution"
fi

# Arithmetic
if [ "$($SHELL_TO_TEST -c 'echo $((2 + 3))' 2>/dev/null)" = "5" ]; then
    echo "✅ Arithmetic expansion"
else
    echo "❌ Arithmetic expansion"
fi

# Here documents
if [ "$($SHELL_TO_TEST -c 'cat << EOF
test
EOF' 2>/dev/null)" = "test" ]; then
    echo "✅ Here documents"
else
    echo "❌ Here documents"
fi

echo ""
echo "Advanced features:"

# Parameter expansion
if [ "$($SHELL_TO_TEST -c 'echo ${UNDEFINED:-default}' 2>/dev/null)" = "default" ]; then
    echo "✅ Parameter expansion"
else
    echo "❌ Parameter expansion"
fi

# Control structures
if [ "$($SHELL_TO_TEST -c 'if true; then echo success; fi' 2>/dev/null)" = "success" ]; then
    echo "✅ Control structures"
else
    echo "❌ Control structures"
fi

# Functions
if [ "$($SHELL_TO_TEST -c 'func() { echo works; }; func' 2>/dev/null)" = "works" ]; then
    echo "✅ Functions"
else
    echo "❌ Functions"
fi

echo ""
echo "Core builtins:"

# echo
if [ "$($SHELL_TO_TEST -c 'echo hello' 2>/dev/null)" = "hello" ]; then
    echo "✅ echo"
else
    echo "❌ echo"
fi

# cd
if $SHELL_TO_TEST -c 'cd /tmp && pwd' >/dev/null 2>&1; then
    echo "✅ cd"
else
    echo "❌ cd"
fi

# export
if [ "$($SHELL_TO_TEST -c 'export VAR=test; echo $VAR' 2>/dev/null)" = "test" ]; then
    echo "✅ export"
else
    echo "❌ export"
fi

# test
if $SHELL_TO_TEST -c 'test -f /etc/passwd' >/dev/null 2>&1; then
    echo "✅ test"
else
    echo "❌ test"
fi

# eval
if [ "$($SHELL_TO_TEST -c 'CMD="echo works"; eval $CMD' 2>/dev/null)" = "works" ]; then
    echo "✅ eval"
else
    echo "❌ eval"
fi

echo ""
echo "=== Quick Test Complete ==="
echo ""
echo "For comprehensive testing, run:"
echo "  ./tests/run_shell_tests.sh"
echo ""
echo "For individual POSIX test suites:"
echo "  ./tests/shell/test_posix_compliance.sh $SHELL_TO_TEST"
echo "  ./tests/shell/test_posix_advanced.sh $SHELL_TO_TEST"
echo "  ./tests/shell/test_posix_builtins.sh $SHELL_TO_TEST"
