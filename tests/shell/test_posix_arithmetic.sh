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
echo "Testing POSIX Arithmetic Expansion for: $SHELL_TO_TEST"
echo "===================================================="
log_test "Basic arithmetic expansion \$((2+3))"
result=$("$SHELL_TO_TEST" -c "echo \$((2+3))" 2>/dev/null)
if [ "$result" = "5" ]; then
    pass
else
    fail "Expected '5', got '$result'"
fi
log_test "Arithmetic with variables"
result=$("$SHELL_TO_TEST" -c "a=10; b=5; echo \$((a+b))" 2>/dev/null)
if [ "$result" = "15" ]; then
    pass
else
    fail "Expected '15', got '$result'"
fi
log_test "Arithmetic subtraction"
result=$("$SHELL_TO_TEST" -c "echo \$((10-3))" 2>/dev/null)
if [ "$result" = "7" ]; then
    pass
else
    fail "Expected '7', got '$result'"
fi
log_test "Arithmetic multiplication"
result=$("$SHELL_TO_TEST" -c "echo \$((4*5))" 2>/dev/null)
if [ "$result" = "20" ]; then
    pass
else
    fail "Expected '20', got '$result'"
fi
log_test "Arithmetic division"
result=$("$SHELL_TO_TEST" -c "echo \$((15/3))" 2>/dev/null)
if [ "$result" = "5" ]; then
    pass
else
    fail "Expected '5', got '$result'"
fi
log_test "Arithmetic modulo"
result=$("$SHELL_TO_TEST" -c "echo \$((17%5))" 2>/dev/null)
if [ "$result" = "2" ]; then
    pass
else
    fail "Expected '2', got '$result'"
fi
log_test "Parentheses in arithmetic"
result=$("$SHELL_TO_TEST" -c "echo \$((2*(3+4)))" 2>/dev/null)
if [ "$result" = "14" ]; then
    pass
else
    fail "Expected '14', got '$result'"
fi
log_test "Negative numbers"
result=$("$SHELL_TO_TEST" -c "echo \$((-5+10))" 2>/dev/null)
if [ "$result" = "5" ]; then
    pass
else
    fail "Expected '5', got '$result'"
fi
log_test "Comparison equal (==)"
result=$("$SHELL_TO_TEST" -c "echo \$((5==5))" 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi
log_test "Comparison not equal (!=)"
result=$("$SHELL_TO_TEST" -c "echo \$((5!=3))" 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi
log_test "Comparison less than (<)"
result=$("$SHELL_TO_TEST" -c "echo \$((3<5))" 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi
log_test "Comparison greater than (>)"
result=$("$SHELL_TO_TEST" -c "echo \$((7>3))" 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi
log_test "Logical AND (&&)"
result=$("$SHELL_TO_TEST" -c "echo \$((1&&1))" 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi
log_test "Logical OR (||)"
result=$("$SHELL_TO_TEST" -c "echo \$((0||1))" 2>/dev/null)
if [ "$result" = "1" ]; then
    pass
else
    fail "Expected '1', got '$result'"
fi
log_test "Bitwise AND (&)"
result=$("$SHELL_TO_TEST" -c "echo \$((12&10))" 2>/dev/null)
if [ "$result" = "8" ]; then
    pass
else
    fail "Expected '8', got '$result'"
fi
log_test "Bitwise OR (|)"
result=$("$SHELL_TO_TEST" -c "echo \$((12|10))" 2>/dev/null)
if [ "$result" = "14" ]; then
    pass
else
    fail "Expected '14', got '$result'"
fi
log_test "Bitwise XOR (^)"
result=$("$SHELL_TO_TEST" -c "echo \$((12^10))" 2>/dev/null)
if [ "$result" = "6" ]; then
    pass
else
    fail "Expected '6', got '$result'"
fi
log_test "Left shift (<<)"
result=$("$SHELL_TO_TEST" -c "echo \$((3<<2))" 2>/dev/null)
if [ "$result" = "12" ]; then
    pass
else
    fail "Expected '12', got '$result'"
fi
log_test "Right shift (>>)"
result=$("$SHELL_TO_TEST" -c "echo \$((12>>2))" 2>/dev/null)
if [ "$result" = "3" ]; then
    pass
else
    fail "Expected '3', got '$result'"
fi
log_test "Assignment in arithmetic"
result=$("$SHELL_TO_TEST" -c "a=5; echo \$((a+=3)); echo \$a" 2>/dev/null | tr '\n' ' ')
expected="8 8 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi
log_test "Pre-increment (++var)"
result=$("$SHELL_TO_TEST" -c "a=5; echo \$((++a)); echo \$a" 2>/dev/null | tr '\n' ' ')
expected="6 6 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi
log_test "Post-increment (var++)"
result=$("$SHELL_TO_TEST" -c "a=5; echo \$((a++)); echo \$a" 2>/dev/null | tr '\n' ' ')
expected="5 6 "
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Expected '$expected', got '$result'"
fi
log_test "Ternary operator (?:)"
result=$("$SHELL_TO_TEST" -c "echo \$((5>3?10:20))" 2>/dev/null)
if [ "$result" = "10" ]; then
    pass
else
    fail "Expected '10', got '$result'"
fi
log_test "Complex arithmetic expression"
result=$("$SHELL_TO_TEST" -c "a=2; b=3; c=4; echo \$((a*b+c*(a+1)))" 2>/dev/null)
if [ "$result" = "18" ]; then
    pass
else
    fail "Expected '18', got '$result'"
fi
log_test "Zero division error handling"
"$SHELL_TO_TEST" -c "echo \$((5/0))" >/dev/null 2>&1
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    fail "Should have failed with non-zero exit code for division by zero"
fi
echo
echo "Arithmetic Expansion Test Summary:"
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All arithmetic expansion tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED arithmetic expansion tests failed${NC}"
    exit 1
fi
