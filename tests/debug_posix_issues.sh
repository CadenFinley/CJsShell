#!/usr/bin/env sh
# Specific tests for the failing POSIX compliance areas

SHELL_TO_TEST="${1:-./build/cjsh}"

echo "Testing specific POSIX compliance issues..."
echo "==========================================="

# Test here documents in detail
echo "Testing here documents..."
echo "1. Basic here document:"
result=$("$SHELL_TO_TEST" -c 'cat << EOF
This is line 1
This is line 2
EOF')
echo "Result: '$result'"

echo ""
echo "2. Here document with variable expansion:"
result=$("$SHELL_TO_TEST" -c 'var=test; cat << EOF
Variable: $var
End
EOF')
echo "Result: '$result'"

echo ""
echo "3. Here document with quoted delimiter (no expansion):"
result=$("$SHELL_TO_TEST" -c 'var=test; cat << "EOF"
Variable: $var
End
EOF')
echo "Result: '$result'"

echo ""
echo "Testing error redirection..."
echo "1. Basic stderr redirection:"
"$SHELL_TO_TEST" -c 'echo "error message" >&2 2> /tmp/test_stderr'
if [ -f /tmp/test_stderr ]; then
    echo "Stderr file created: $(cat /tmp/test_stderr)"
    rm -f /tmp/test_stderr
else
    echo "Stderr redirection failed"
fi

echo ""
echo "2. Combining stdout and stderr:"
result=$("$SHELL_TO_TEST" -c '(echo stdout; echo stderr >&2) 2>&1')
echo "Combined result: '$result'"

echo ""
echo "Testing pipeline with stderr:"
result=$("$SHELL_TO_TEST" -c 'echo stdout; echo stderr >&2' 2>&1 | wc -l)
echo "Line count (should be 2): '$result'"
