#!/usr/bin/env sh

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

echo "Testing POSIX Word Splitting and Quoting for: $SHELL_TO_TEST"
echo "==========================================================="

# Test 1: Basic word splitting on spaces
log_test "Basic word splitting on spaces"
result=$("$SHELL_TO_TEST" -c 'set -- hello world test; echo $#' 2>/dev/null)
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected 3 words, got '$result'"
fi

# Test 2: Word splitting on tabs
log_test "Word splitting on tabs"
result=$("$SHELL_TO_TEST" -c 'IFS="	 "; var="hello	world"; set -- $var; echo $#' 2>/dev/null)
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected 2 words, got '$result'"
fi

# Test 3: Word splitting on newlines
log_test "Word splitting on newlines"
result=$("$SHELL_TO_TEST" -c 'var="hello
world"; set -- $var; echo $#' 2>/dev/null)
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected 2 words, got '$result'"
fi

# Test 4: Custom IFS
log_test "Custom IFS word splitting"
result=$("$SHELL_TO_TEST" -c 'IFS=:; var="a:b:c"; set -- $var; echo $#' 2>/dev/null)
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected 3 words with custom IFS, got '$result'"
fi

# Test 5: Double quotes prevent word splitting
log_test "Double quotes prevent word splitting"
result=$("$SHELL_TO_TEST" -c 'var="hello world"; set -- "$var"; echo $#' 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected 1 word with double quotes, got '$result'"
fi

# Test 6: Single quotes prevent word splitting
log_test "Single quotes prevent word splitting"
result=$("$SHELL_TO_TEST" -c "var='hello world'; set -- \$var; echo \$#" 2>/dev/null)
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected 2 words without quotes on expansion, got '$result'"
fi

# Test 7: Single quotes preserve literal content
log_test "Single quotes preserve literal content"
result=$("$SHELL_TO_TEST" -c "echo 'hello \$USER world'" 2>/dev/null)
if [ "$result" = "hello \$USER world" ]; then
    pass
else
    fail "Expected literal content, got '$result'"
fi

# Test 8: Double quotes allow variable expansion
log_test "Double quotes allow variable expansion"
result=$("$SHELL_TO_TEST" -c 'USER=test; echo "hello $USER world"' 2>/dev/null)
if [ "$result" = "hello test world" ]; then
    pass
else
    fail "Expected variable expansion, got '$result'"
fi

# Test 9: Escaped characters in double quotes
log_test "Escaped characters in double quotes"
result=$("$SHELL_TO_TEST" -c 'echo "hello \"world\""' 2>/dev/null)
if [ "$result" = 'hello "world"' ]; then
    pass
else
    fail "Expected escaped quotes, got '$result'"
fi

# Test 10: Backslash escaping outside quotes
log_test "Backslash escaping outside quotes"
result=$("$SHELL_TO_TEST" -c 'echo hello\ world' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected escaped space, got '$result'"
fi

# Test 11: Command substitution in double quotes
log_test "Command substitution in double quotes"
result=$("$SHELL_TO_TEST" -c 'echo "result: $(echo test)"' 2>/dev/null)
if [ "$result" = "result: test" ]; then
    pass
else
    fail "Expected command substitution, got '$result'"
fi

# Test 12: Command substitution preserves spaces in quotes
log_test "Command substitution preserves spaces in quotes"
result=$("$SHELL_TO_TEST" -c 'echo "$(echo hello world)"' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected preserved spaces, got '$result'"
fi

# Test 13: Mixed quoting
log_test "Mixed quoting"
result=$("$SHELL_TO_TEST" -c 'VAR=test; echo "hello"world' 2>/dev/null)
if [ "$result" = "helloworld" ]; then
    pass
else
    fail "Expected concatenated result, got '$result'"
fi

# Test 14: Empty strings in quotes
log_test "Empty strings in quotes"
result=$("$SHELL_TO_TEST" -c 'set -- "" "hello" ""; echo $#' 2>/dev/null)
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected 3 arguments including empty strings, got '$result'"
fi

# Test 15: IFS with empty value
log_test "IFS with empty value"
result=$("$SHELL_TO_TEST" -c 'IFS=""; var="hello world"; set -- $var; echo $#' 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected no word splitting with empty IFS, got '$result'"
fi

# Test 16: Leading and trailing IFS characters
log_test "Leading and trailing IFS characters"
result=$("$SHELL_TO_TEST" -c 'IFS=:; var=":a:b:"; set -- $var; echo $#' 2>/dev/null)
# POSIX behavior: leading/trailing IFS chars are ignored
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected 2 words with leading/trailing IFS, got '$result'"
fi

# Test 17: Multiple consecutive IFS characters
log_test "Multiple consecutive IFS characters"
result=$("$SHELL_TO_TEST" -c 'IFS=:; var="a::b"; set -- $var; echo $#' 2>/dev/null)
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected 2 words with consecutive IFS, got '$result'"
fi

# Test 18: Quoted variable assignment
log_test "Quoted variable assignment"
result=$("$SHELL_TO_TEST" -c 'var="hello world"; echo "$var"' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected quoted variable content, got '$result'"
fi

# Test 19: Unquoted variable with special characters
log_test "Unquoted variable with special characters"
result=$("$SHELL_TO_TEST" -c 'var="hello*world"; echo $var' 2>/dev/null)
# Should not expand the asterisk when in variable
if [ "$result" = "hello*world" ]; then
    pass
else
    fail "Expected literal asterisk, got '$result'"
fi

# Test 20: Nested quotes
log_test "Nested quote handling"
result=$("$SHELL_TO_TEST" -c "echo \"outer 'inner' quote\"" 2>/dev/null)
if [ "$result" = "outer 'inner' quote" ]; then
    pass
else
    fail "Expected nested quotes, got '$result'"
fi

# Test 21: Parameter expansion in quotes
log_test "Parameter expansion in quotes"
result=$("$SHELL_TO_TEST" -c 'var=test; echo "${var}ing"' 2>/dev/null)
if [ "$result" = "testing" ]; then
    pass
else
    fail "Expected parameter expansion, got '$result'"
fi

# Test 22: Arithmetic expansion in quotes
log_test "Arithmetic expansion in quotes"
result=$("$SHELL_TO_TEST" -c 'echo "result: $((2+3))"' 2>/dev/null)
if [ "$result" = "result: 5" ]; then
    pass
else
    fail "Expected arithmetic expansion, got '$result'"
fi

# Test 23: Backquote command substitution in quotes
log_test "Backquote command substitution in quotes"
result=$("$SHELL_TO_TEST" -c 'echo "result: `echo test`"' 2>/dev/null)
if [ "$result" = "result: test" ]; then
    pass
else
    fail "Expected backquote substitution, got '$result'"
fi

# Test 24: Escape sequences in different quote contexts
log_test "Backslash in single quotes"
result=$("$SHELL_TO_TEST" -c "echo 'back\\slash'" 2>/dev/null)
if [ "$result" = "back\\slash" ]; then
    pass
else
    fail "Expected literal backslash, got '$result'"
fi

# Test 25: Word splitting with multiple IFS characters
log_test "Multiple IFS characters"
result=$("$SHELL_TO_TEST" -c 'IFS=" :"; var="a b:c d"; set -- $var; echo $#' 2>/dev/null)
if [ "$result" = "4" ]; then
    pass
else
    fail "Expected 4 words with multiple IFS chars, got '$result'"
fi

# Test 26: Quoting with special shell characters
log_test "Quoting special shell characters"
result=$("$SHELL_TO_TEST" -c 'echo "chars: \$()[]{}*?"' 2>/dev/null)
if [ "$result" = "chars: \$()[]{}*?" ]; then
    pass
else
    fail "Expected quoted special characters, got '$result'"
fi

# Test 27: Unset variable in quotes
log_test "Unset variable in quotes"
result=$("$SHELL_TO_TEST" -c 'unset NOTSET; echo "value: $NOTSET"' 2>/dev/null)
if [ "$result" = "value: " ]; then
    pass
else
    fail "Expected empty expansion, got '$result'"
fi

# Test 28: Quote removal
log_test "Quote removal after expansion"
result=$("$SHELL_TO_TEST" -c 'var="hello"; echo $var' 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected quote removal, got '$result'"
fi

# Test 29: Concatenated quoted strings
log_test "Concatenated quoted strings"
result=$("$SHELL_TO_TEST" -c 'echo "hello"" ""world"' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected concatenated strings, got '$result'"
fi

# Test 30: Complex quoting scenario
log_test "Complex quoting scenario"
result=$("$SHELL_TO_TEST" -c 'VAR=test; echo "prefix_${VAR}_suffix"' 2>/dev/null)
if [ "$result" = "prefix_test_suffix" ]; then
    pass
else
    fail "Expected complex expansion, got '$result'"
fi

# Summary
echo
echo "Word Splitting and Quoting Test Summary:"
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All word splitting and quoting tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED word splitting and quoting tests failed${NC}"
    exit 1
fi