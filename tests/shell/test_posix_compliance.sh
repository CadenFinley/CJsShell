#!/usr/bin/env sh
# POSIX Compliance Test Suite for CJ's Shell
# Tests core POSIX shell features and behaviors

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TOTAL=0
PASSED=0
FAILED=0

# Shell to test
SHELL_TO_TEST="${1:-./build/cjsh}"

log_test() {
    TOTAL=$((TOTAL + 1))
    printf "Test %03d: %s... " "$TOTAL" "$1"
}

pass() {
    PASSED=$((PASSED + 1))
    printf "${GREEN}PASS${NC}\n"
}

fail() {
    FAILED=$((FAILED + 1))
    printf "${RED}FAIL${NC} - %s\n" "$1"
}

# Check if shell exists
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing POSIX compliance for: $SHELL_TO_TEST"
echo "================================================"

# Test 1: Basic command execution
log_test "Basic command execution"
result=$("$SHELL_TO_TEST" -c "echo hello" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected 'hello', got '$result'"
fi

# Test 2: Exit status propagation
log_test "Exit status propagation"
"$SHELL_TO_TEST" -c "false" 2>/dev/null
if [ $? -eq 1 ]; then
    pass
else
    fail "Expected exit status 1 from false command"
fi

# Test 3: Environment variable expansion
log_test "Environment variable expansion"
result=$("$SHELL_TO_TEST" -c "TEST_VAR=hello; echo \$TEST_VAR" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected 'hello', got '$result'"
fi

# Test 4: Command substitution (backticks)
log_test "Command substitution with backticks"
result=$("$SHELL_TO_TEST" -c "echo \`echo nested\`" 2>/dev/null)
if [ "$result" = "nested" ]; then
    pass
else
    fail "Expected 'nested', got '$result'"
fi

# Test 5: Command substitution ($())
log_test "Command substitution with \$()"
result=$("$SHELL_TO_TEST" -c "echo \$(echo nested)" 2>/dev/null)
if [ "$result" = "nested" ]; then
    pass
else
    fail "Expected 'nested', got '$result'"
fi

# Test 6: Simple pipeline
log_test "Simple pipeline"
result=$("$SHELL_TO_TEST" -c "echo hello | cat" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected 'hello', got '$result'"
fi

# Test 7: Logical AND (&&)
log_test "Logical AND (&&)"
result=$("$SHELL_TO_TEST" -c "true && echo success" 2>/dev/null)
if [ "$result" = "success" ]; then
    pass
else
    fail "Expected 'success', got '$result'"
fi

# Test 8: Logical OR (||)
log_test "Logical OR (||)"
result=$("$SHELL_TO_TEST" -c "false || echo success" 2>/dev/null)
if [ "$result" = "success" ]; then
    pass
else
    fail "Expected 'success', got '$result'"
fi

# Test 9: Background jobs (&)
log_test "Background jobs (&)"
"$SHELL_TO_TEST" -c "sleep 0.1 &" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Background job execution failed"
fi

# Test 10: I/O redirection (>)
log_test "Output redirection (>)"
"$SHELL_TO_TEST" -c "echo test > /tmp/posix_test_$$" 2>/dev/null
if [ -f "/tmp/posix_test_$$" ] && [ "$(cat /tmp/posix_test_$$)" = "test" ]; then
    pass
    rm -f "/tmp/posix_test_$$"
else
    fail "Output redirection failed"
    rm -f "/tmp/posix_test_$$"
fi

# Test 11: Input redirection (<)
log_test "Input redirection (<)"
echo "input_test" > "/tmp/posix_input_$$"
result=$("$SHELL_TO_TEST" -c "cat < /tmp/posix_input_$$" 2>/dev/null)
if [ "$result" = "input_test" ]; then
    pass
else
    fail "Expected 'input_test', got '$result'"
fi
rm -f "/tmp/posix_input_$$"

# Test 12: Append redirection (>>)
log_test "Append redirection (>>)"
"$SHELL_TO_TEST" -c "echo line1 > /tmp/posix_append_$$; echo line2 >> /tmp/posix_append_$$" 2>/dev/null
result=$(cat "/tmp/posix_append_$$" 2>/dev/null)
expected="line1
line2"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Append redirection failed"
fi
rm -f "/tmp/posix_append_$$"

# Test 13: cd builtin
log_test "cd builtin command"
result=$("$SHELL_TO_TEST" -c "cd /tmp && pwd" 2>/dev/null)
if [ "$result" = "/tmp" ] || [ "$result" = "/private/tmp" ]; then
    pass
else
    fail "cd command failed, expected '/tmp', got '$result'"
fi

# Test 14: export builtin
log_test "export builtin command"
result=$("$SHELL_TO_TEST" -c "export TEST_EXPORT=exported; echo \$TEST_EXPORT" 2>/dev/null)
if [ "$result" = "exported" ]; then
    pass
else
    fail "export command failed"
fi

# Test 15: test/[ builtin
log_test "test/[ builtin command"
"$SHELL_TO_TEST" -c "test -f /etc/passwd" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "test command failed"
fi

# Test 16: Bracket test command
log_test "Bracket [ ] test command"
"$SHELL_TO_TEST" -c "[ -f /etc/passwd ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "bracket test command failed"
fi

# Test 17: String comparison in test
log_test "String comparison in test"
"$SHELL_TO_TEST" -c "[ \"hello\" = \"hello\" ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "string comparison in test failed"
fi

# Test 18: Numeric comparison in test
log_test "Numeric comparison in test"
"$SHELL_TO_TEST" -c "[ 5 -eq 5 ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "numeric comparison in test failed"
fi

# Test 19: File existence test
log_test "File existence test (-f)"
"$SHELL_TO_TEST" -c "[ -f /etc/passwd ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "file existence test failed"
fi

# Test 20: Directory existence test
log_test "Directory existence test (-d)"
"$SHELL_TO_TEST" -c "[ -d /tmp ]" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "directory existence test failed"
fi

# Test 21: Variable assignment
log_test "Variable assignment"
result=$("$SHELL_TO_TEST" -c "var=value; echo \$var" 2>/dev/null)
if [ "$result" = "value" ]; then
    pass
else
    fail "Variable assignment failed"
fi

# Test 22: Single quotes (literal strings)
log_test "Single quotes (literal strings)"
result=$("$SHELL_TO_TEST" -c "echo 'hello \$USER world'" 2>/dev/null)
if [ "$result" = "hello \$USER world" ]; then
    pass
else
    fail "Single quotes not handled correctly"
fi

# Test 23: Double quotes (variable expansion)
log_test "Double quotes (variable expansion)"
result=$("$SHELL_TO_TEST" -c "var=test; echo \"hello \$var world\"" 2>/dev/null)
if [ "$result" = "hello test world" ]; then
    pass
else
    fail "Double quotes not handled correctly"
fi

# Test 24: Escape sequences
log_test "Escape sequences"
result=$("$SHELL_TO_TEST" -c "echo hello\\ world" 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Escape sequences not handled correctly"
fi

# Test 25: Command separator (;)
log_test "Command separator (;)"
result=$("$SHELL_TO_TEST" -c "echo first; echo second" 2>/dev/null)
expected="first
second"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Command separator not working correctly"
fi

# Test 26: Here document (<<)
log_test "Here document (<<)"
result=$("$SHELL_TO_TEST" -c "cat << EOF
line1
line2
EOF" 2>/dev/null)
expected="line1
line2"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Here document not working correctly"
fi

# Test 27: Tilde expansion
log_test "Tilde expansion (~)"
result=$("$SHELL_TO_TEST" -c "echo ~" 2>/dev/null)
if [ "$result" = "$HOME" ]; then
    pass
else
    fail "Tilde expansion failed, expected '$HOME', got '$result'"
fi

# Test 28: Job control - disown
log_test "Job control functionality"
"$SHELL_TO_TEST" -c "true &" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Basic job control failed"
fi

# Test 29: Error redirection (2>)
log_test "Error redirection (2>)"
"$SHELL_TO_TEST" -c "echo error >&2 2> /tmp/error_test_$$" 2>/dev/null
if [ -f "/tmp/error_test_$$" ] && [ "$(cat /tmp/error_test_$$)" = "error" ]; then
    pass
    rm -f "/tmp/error_test_$$"
else
    fail "Error redirection failed"
    rm -f "/tmp/error_test_$$"
fi

# Test 30: Combine stdout and stderr (2>&1)
log_test "Combine stdout and stderr (2>&1)"
result=$("$SHELL_TO_TEST" -c "(echo stdout; echo stderr >&2) 2>&1 | cat" 2>/dev/null)
# Should contain both stdout and stderr
if echo "$result" | grep -q "stdout" && echo "$result" | grep -q "stderr"; then
    pass
else
    fail "Combining stdout and stderr failed"
fi

echo "================================================"
echo "POSIX Compliance Test Results:"
echo "Total tests: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All tests passed! Your shell appears to be POSIX compliant.${NC}"
    exit 0
else
    echo "${YELLOW}Some tests failed. Review the failures above.${NC}"
    echo "Success rate: $(( PASSED * 100 / TOTAL ))%"
    exit 1
fi
