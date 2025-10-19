#!/usr/bin/env sh


GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

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

echo "Testing 'function' Keyword Support for: $SHELL_TO_TEST"
echo "========================================================"

log_test "Traditional name() { ... } syntax"
result=$("$SHELL_TO_TEST" -c 'hello() { echo "world"; }; hello' 2>/dev/null)
if [ "$result" = "world" ]; then
    pass
else
    fail "Expected 'world', got '$result'"
fi

log_test "function name { ... } syntax"
result=$("$SHELL_TO_TEST" -c 'function hello { echo "world"; }; hello' 2>/dev/null)
if [ "$result" = "world" ]; then
    pass
else
    fail "Expected 'world', got '$result'"
fi

log_test "function name() { ... } syntax"
result=$("$SHELL_TO_TEST" -c 'function hello() { echo "world"; }; hello' 2>/dev/null)
if [ "$result" = "world" ]; then
    pass
else
    fail "Expected 'world', got '$result'"
fi

log_test "function keyword with parameters"
result=$("$SHELL_TO_TEST" -c 'function greet { echo "Hello $1"; }; greet Alice' 2>/dev/null)
if [ "$result" = "Hello Alice" ]; then
    pass
else
    fail "Expected 'Hello Alice', got '$result'"
fi

log_test "function keyword with multiple parameters"
result=$("$SHELL_TO_TEST" -c 'function add { echo $(($1 + $2)); }; add 10 20' 2>/dev/null)
if [ "$result" = "30" ]; then
    pass
else
    fail "Expected '30', got '$result'"
fi

log_test "function keyword with return value"
result=$("$SHELL_TO_TEST" -c 'function test_ret { return 7; }; test_ret; echo $?' 2>/dev/null)
if [ "$result" = "7" ]; then
    pass
else
    fail "Expected '7', got '$result'"
fi

log_test "function keyword with multiline body"
result=$("$SHELL_TO_TEST" -c 'function multi {
    echo "line1"
    echo "line2"
    echo "line3"
}; multi' 2>/dev/null | tr '\n' ' ')
expected="line1 line2 line3 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword with recursion"
result=$("$SHELL_TO_TEST" -c 'function countdown {
    if [ $1 -gt 0 ]; then
        echo $1
        countdown $(($1-1))
    fi
}; countdown 3' 2>/dev/null | tr '\n' ' ')
expected="3 2 1 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword - nested function calls"
result=$("$SHELL_TO_TEST" -c 'function inner { echo "inner"; }; function outer { inner; echo "outer"; }; outer' 2>/dev/null | tr '\n' ' ')
expected="inner outer "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "Mixed function syntaxes in same script"
result=$("$SHELL_TO_TEST" -c 'func1() { echo "trad"; }; function func2 { echo "keyword"; }; function func3() { echo "both"; }; func1; func2; func3' 2>/dev/null | tr '\n' ' ')
expected="trad keyword both "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword with single-line syntax"
result=$("$SHELL_TO_TEST" -c 'function oneline { echo "single"; }; oneline' 2>/dev/null)
if [ "$result" = "single" ]; then
    pass
else
    fail "Expected 'single', got '$result'"
fi

log_test "function keyword with command substitution"
result=$("$SHELL_TO_TEST" -c 'function get_date { echo $(date +%Y); }; get_date' 2>/dev/null)
if [ -n "$result" ] && [ "$result" = "2025" ] || [ "$result" -ge 2024 ]; then
    pass
else
    fail "Expected year output, got '$result'"
fi

log_test "function keyword with pipeline"
result=$("$SHELL_TO_TEST" -c 'function count_lines { echo -e "a\nb\nc" | wc -l; }; count_lines' 2>/dev/null | tr -d ' ')
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected '3', got '$result'"
fi

log_test "function keyword with if statement"
result=$("$SHELL_TO_TEST" -c 'function check {
    if [ "$1" = "yes" ]; then
        echo "affirmative"
    else
        echo "negative"
    fi
}; check yes' 2>/dev/null)
if [ "$result" = "affirmative" ]; then
    pass
else
    fail "Expected 'affirmative', got '$result'"
fi

log_test "function keyword with for loop"
result=$("$SHELL_TO_TEST" -c 'function loop_test {
    for i in 1 2 3; do
        echo $i
    done
}; loop_test' 2>/dev/null | tr '\n' ' ')
expected="1 2 3 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword with case statement"
result=$("$SHELL_TO_TEST" -c 'function check_val {
    case $1 in
        a) echo "alpha" ;;
        b) echo "beta" ;;
        *) echo "other" ;;
    esac
}; check_val a' 2>/dev/null)
if [ "$result" = "alpha" ]; then
    pass
else
    fail "Expected 'alpha', got '$result'"
fi

log_test "function keyword with $# and $@"
result=$("$SHELL_TO_TEST" -c 'function count_args {
    echo "Count: $#"
    echo "Args: $@"
}; count_args one two three' 2>/dev/null | tr '\n' ' ')
expected="Count: 3 Args: one two three "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword with variable assignment"
result=$("$SHELL_TO_TEST" -c 'function set_var {
    myvar="test123"
    echo $myvar
}; set_var' 2>/dev/null)
if [ "$result" = "test123" ]; then
    pass
else
    fail "Expected 'test123', got '$result'"
fi

log_test "function keyword - redefining function"
result=$("$SHELL_TO_TEST" -c 'function test_func { echo "first"; }; function test_func { echo "second"; }; test_func' 2>/dev/null)
if [ "$result" = "second" ]; then
    pass
else
    fail "Expected 'second', got '$result'"
fi

log_test "function keyword with arithmetic expansion"
result=$("$SHELL_TO_TEST" -c 'function calc { echo $((($1 * $2) + $3)); }; calc 5 4 3' 2>/dev/null)
if [ "$result" = "23" ]; then
    pass
else
    fail "Expected '23', got '$result'"
fi

log_test "function keyword with subshell"
result=$("$SHELL_TO_TEST" -c 'x=outer; function subtest { (x=inner; echo $x); echo $x; }; subtest' 2>/dev/null | tr '\n' ' ')
expected="inner outer "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword - exit code from command"
result=$("$SHELL_TO_TEST" -c 'function fail_test { false; }; fail_test; echo $?' 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi

log_test "function keyword with quoted parameters"
result=$("$SHELL_TO_TEST" -c 'function show_quote { echo "$1"; }; show_quote "hello world"' 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Expected 'hello world', got '$result'"
fi

log_test "function keyword with semicolon on same line"
result=$("$SHELL_TO_TEST" -c 'function semi { echo "test"; }; semi' 2>/dev/null)
if [ "$result" = "test" ]; then
    pass
else
    fail "Expected 'test', got '$result'"
fi

log_test "function definition followed by command"
result=$("$SHELL_TO_TEST" -c 'function first { echo "1"; }; echo "2"; first' 2>/dev/null | tr '\n' ' ')
expected="2 1 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword with empty body"
result=$("$SHELL_TO_TEST" -c 'function empty { :; }; empty; echo $?' 2>/dev/null)
if [ "$result" = "0" ]; then
    pass
else
    fail "Expected '0', got '$result'"
fi

log_test "function keyword with underscore in name"
result=$("$SHELL_TO_TEST" -c 'function my_func_name { echo "underscore"; }; my_func_name' 2>/dev/null)
if [ "$result" = "underscore" ]; then
    pass
else
    fail "Expected 'underscore', got '$result'"
fi

log_test "function keyword with numbers in name"
result=$("$SHELL_TO_TEST" -c 'function func123 { echo "numbers"; }; func123' 2>/dev/null)
if [ "$result" = "numbers" ]; then
    pass
else
    fail "Expected 'numbers', got '$result'"
fi

log_test "Multiple sequential function definitions"
result=$("$SHELL_TO_TEST" -c 'function a { echo "A"; }; function b { echo "B"; }; function c { echo "C"; }; a; b; c' 2>/dev/null | tr '\n' ' ')
expected="A B C "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi

log_test "function keyword with background execution"
result=$("$SHELL_TO_TEST" -c 'function bg_test { sleep 0.1; echo "done"; }; bg_test &' 2>/dev/null)
if [ $? -eq 0 ]; then
    pass
else
    fail "Background execution failed"
fi

echo
echo "Function Keyword Test Summary:"
echo "=============================="
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All function keyword tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED function keyword tests failed${NC}"
    exit 1
fi
