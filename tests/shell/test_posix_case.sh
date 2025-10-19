#!/usr/bin/env sh
TOTAL=0
PASSED=0
FAILED=0
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
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi
echo "Testing POSIX Case Statement for: $SHELL_TO_TEST"
echo "==============================================="
log_test "Basic case statement"
result=$("$SHELL_TO_TEST" -c 'var=hello; case $var in hello) echo match;; *) echo no_match;; esac' 2>/dev/null)
if [ "$result" = "match" ]; then
    pass
else
    fail "Expected 'match', got '$result'"
fi
log_test "Case with wildcard pattern"
result=$("$SHELL_TO_TEST" -c 'var=test123; case $var in test*) echo wildcard_match;; *) echo no_match;; esac' 2>/dev/null)
if [ "$result" = "wildcard_match" ]; then
    pass
else
    fail "Expected 'wildcard_match', got '$result'"
fi
log_test "Case with character class"
result=$("$SHELL_TO_TEST" -c 'var=a; case $var in [a-z]) echo letter;; *) echo not_letter;; esac' 2>/dev/null)
if [ "$result" = "letter" ]; then
    pass
else
    fail "Expected 'letter', got '$result'"
fi
log_test "Case with multiple patterns"
result=$("$SHELL_TO_TEST" -c 'var=two; case $var in one|two|three) echo number_word;; *) echo other;; esac' 2>/dev/null)
if [ "$result" = "number_word" ]; then
    pass
else
    fail "Expected 'number_word', got '$result'"
fi
log_test "Case with no match (default case)"
result=$("$SHELL_TO_TEST" -c 'var=xyz; case $var in hello) echo match;; *) echo default;; esac' 2>/dev/null)
if [ "$result" = "default" ]; then
    pass
else
    fail "Expected 'default', got '$result'"
fi
log_test "Case with multiple commands"
result=$("$SHELL_TO_TEST" -c 'var=test; case $var in test) echo first; echo second;; esac' 2>/dev/null | tr '\n' ' ')
expected="first second "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi
log_test "Case statement structure"
result=$("$SHELL_TO_TEST" -c 'var=a; case $var in [a-z]) echo lower;; [A-Z]) echo upper;; esac' 2>/dev/null)
if [ "$result" = "lower" ]; then
    pass
else
    fail "Expected 'lower', got '$result'"
fi
log_test "Nested case statements"
result=$("$SHELL_TO_TEST" -c 'outer=a; inner=1; case $outer in a) case $inner in 1) echo nested_match;; esac;; esac' 2>/dev/null)
if [ "$result" = "nested_match" ]; then
    pass
else
    fail "Expected 'nested_match', got '$result'"
fi
log_test "Case with quoted pattern"
result=$("$SHELL_TO_TEST" -c 'var="hello world"; case "$var" in "hello world") echo quoted_match;; *) echo no_match;; esac' 2>/dev/null)
if [ "$result" = "quoted_match" ]; then
    pass
else
    fail "Expected 'quoted_match', got '$result'"
fi
log_test "Case with empty string"
result=$("$SHELL_TO_TEST" -c 'var=""; case $var in "") echo empty;; *) echo not_empty;; esac' 2>/dev/null)
if [ "$result" = "empty" ]; then
    pass
else
    fail "Expected 'empty', got '$result'"
fi
log_test "Case with special characters in pattern"
result=$("$SHELL_TO_TEST" -c 'var="file.txt"; case $var in *.txt) echo text_file;; *) echo other_file;; esac' 2>/dev/null)
if [ "$result" = "text_file" ]; then
    pass
else
    fail "Expected 'text_file', got '$result'"
fi
log_test "Case with numeric patterns"
result=$("$SHELL_TO_TEST" -c 'var=42; case $var in [0-9]*) echo number;; *) echo not_number;; esac' 2>/dev/null)
if [ "$result" = "number" ]; then
    pass
else
    fail "Expected 'number', got '$result'"
fi
log_test "Case with question mark pattern"
result=$("$SHELL_TO_TEST" -c 'var=cat; case $var in c?t) echo three_letter;; *) echo other;; esac' 2>/dev/null)
if [ "$result" = "three_letter" ]; then
    pass
else
    fail "Expected 'three_letter', got '$result'"
fi
log_test "Case with bracket patterns"
result=$("$SHELL_TO_TEST" -c 'var=5; case $var in [1-5]) echo in_range;; *) echo out_of_range;; esac' 2>/dev/null)
if [ "$result" = "in_range" ]; then
    pass
else
    fail "Expected 'in_range', got '$result'"
fi
log_test "Case with negated bracket patterns"
result=$("$SHELL_TO_TEST" -c 'var=z; case $var in [!a-m]) echo not_early_letter;; *) echo early_letter;; esac' 2>/dev/null)
if [ "$result" = "not_early_letter" ]; then
    pass
else
    fail "Expected 'not_early_letter', got '$result'"
fi
log_test "Case with command substitution in value"
result=$("$SHELL_TO_TEST" -c 'case $(echo hello) in hello) echo cmd_sub_match;; *) echo no_match;; esac' 2>/dev/null)
if [ "$result" = "cmd_sub_match" ]; then
    pass
else
    fail "Expected 'cmd_sub_match', got '$result'"
fi
log_test "Case statement with variables"
result=$("$SHELL_TO_TEST" -c 'pattern=test; var=test; case $var in $pattern) echo var_pattern_match;; *) echo no_match;; esac' 2>/dev/null)
if [ "$result" = "var_pattern_match" ]; then
    pass
else
    fail "Expected 'var_pattern_match', got '$result'"
fi
log_test "Case statement exit status"
"$SHELL_TO_TEST" -c 'var=match; case $var in match) true;; *) false;; esac' 2>/dev/null
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass
else
    fail "Expected exit code 0, got $exit_code"
fi
log_test "Case with complex pattern"
result=$("$SHELL_TO_TEST" -c 'var=abc123; case $var in [a-z]*[0-9]) echo complex_match;; *) echo no_match;; esac' 2>/dev/null)
if [ "$result" = "complex_match" ]; then
    pass
else
    fail "Expected 'complex_match', got '$result'"
fi
log_test "Case with escaped characters"
result=$("$SHELL_TO_TEST" -c 'var="*"; case $var in \\*) echo escaped_star;; *) echo literal_star;; esac' 2>/dev/null)
if [ "$result" = "escaped_star" ]; then
    pass
else
    fail "Expected 'escaped_star', got '$result'"
fi
echo
echo "Case Statement Test Summary:"
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All case statement tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED case statement tests failed${NC}"
    exit 1
fi
