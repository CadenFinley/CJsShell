#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
if [ ! -x "$CJSH_PATH" ]; then
    echo "FAIL: cjsh binary not found at $CJSH_PATH"
    echo "Please build the project before running this test."
    exit 1
fi
TOTAL=0
PASSED=0
FAILED=0
pass() {
    PASSED=$((PASSED + 1))
    printf "PASS: %s\n" "$1"
}
fail() {
    FAILED=$((FAILED + 1))
    printf "FAIL: %s -- %s\n" "$1" "$2"
}
expect_output() {
    desc=$1
    script=$2
    expected=$3
    TOTAL=$((TOTAL + 1))
    output=$("$CJSH_PATH" -c "$script" 2>&1)
    if [ "$output" = "$expected" ]; then
        pass "$desc"
    else
        fail "$desc" "expected: [$expected], got: [$output]"
    fi
}
expect_exit() {
    desc=$1
    script=$2
    expected_exit=$3
    TOTAL=$((TOTAL + 1))
    "$CJSH_PATH" -c "$script" >/dev/null 2>&1
    exit_code=$?
    if [ "$exit_code" -eq "$expected_exit" ]; then
        pass "$desc"
    else
        fail "$desc" "expected exit $expected_exit, got $exit_code"
    fi
}
expect_output "Arithmetic precedence and grouping" \
    'echo $((2 + 3 * 4 - 5))' \
    "9"
expect_output "Unary minus with nested parentheses" \
    'echo $((-(2 + 3) * 4))' \
    "-20"
expect_output "Hexadecimal and octal literals" \
    'echo $((0xff + 010))' \
    "263"
expect_output "Logical combination of comparisons" \
    'echo $(((5 > 3) && (2 < 1) || (4 == 4)))' \
    "1"
expect_output "Bitwise chain operations" \
    'echo $(((13 & 11) ^ 3))' \
    "10"
expect_output "Shift operations chained" \
    'echo $(((3 << 4) >> 2))' \
    "12"
expect_output "Compound assignment sequence" \
    'a=5; a=$((a + 3)); a=$((a * 2)); echo $a' \
    "16"
expect_output "Pre-increment evaluation" \
    'a=4; a=$((a + 1)); echo "$a $a"' \
    "5 5"
expect_output "Post-increment evaluation" \
    'a=4; original=$a; a=$((a + 1)); echo "$original $a"' \
    "4 5"
expect_output "Pre-decrement inside expression" \
    'a=10; a=$((a - 1)); echo $((8 + a))' \
    "17"
expect_output "Post-decrement returns original" \
    'a=3; original=$a; a=$((a - 1)); echo "$original $a"' \
    "3 2"
expect_output "Arithmetic with exported environment" \
    'export RATE=7; echo $((RATE * 3))' \
    "21"
expect_output "Unset variable defaults to zero" \
    'unset nope; echo $((nope + 5))' \
    "5"
expect_output "Positional parameter arithmetic" \
    'set -- 9 6; echo $(( $1 * $2 ))' \
    "54"
expect_output "Nested arithmetic referencing variable" \
    'X=3; echo $(((X + 2) * 2))' \
    "10"
even_output=$(printf "2\n4\n6\n8\n10\n")
expect_output "For loop even filter using test arithmetic" \
    'for i in {1..10}; do if [ $((i % 2)) -eq 0 ]; then echo $i; fi; done' \
    "$even_output"
while_output=$(printf "5\n3\n1\n")
expect_output "While loop decrement with arithmetic guard" \
    'count=5; while [ $((count)) -gt 0 ]; do echo $count; count=$((count-2)); done' \
    "$while_output"
until_output=$(printf "1\n2\n3\n")
expect_output "Until loop growth with arithmetic guard" \
    'count=0; until [ $((count)) -ge 3 ]; do count=$((count+1)); echo $count; done' \
    "$until_output"
expect_output "Arithmetic command assignment" \
    'a=0; a=$((a + 5)); echo $a' \
    "5"
expect_exit "Arithmetic guard success" 'if [ $((3 * 3 == 9)) -ne 0 ]; then exit 0; else exit 1; fi' 0
expect_exit "Arithmetic guard failure" 'if [ $((0)) -ne 0 ]; then exit 0; else exit 1; fi' 1
expect_output "Ternary operator basic" \
    'a=4; b=7; echo $((a > b ? a : b))' \
    "7"
expect_output "Nested ternary logic" \
    'x=5; y=10; z=15; echo $((x>y?100:(y>z?200:300)))' \
    "300"
expect_output "Conditional assignment from arithmetic comparison" \
    'a=0; if [ $a -eq 0 ]; then a=$((a + 42)); fi; echo $a' \
    "42"
expect_output "Function scoped accumulation" \
    'foo(){ sum=0; for i in 1 2 3 4; do sum=$((sum+i)); done; echo $sum; }; foo' \
    "10"
expect_output "Function arithmetic updates global" \
    'counter=1; foo(){ counter=$((counter+1)); }; foo; result=$counter; unset counter; echo $result' \
    "2"
sum_output=$(printf "10\n10\n")
expect_output "Augmented assignment echoes new value" \
    'value=3; echo $((value+=7)); echo $value' \
    "$sum_output"
expect_output "Deeply nested parentheses spacing" \
    'echo $((( (10) - (3) ) * (2 + 1)))' \
    "21"
squares_output=$(printf "1\n4\n9\n")
expect_output "Squared values in list" \
    'for i in 1 2 3; do echo $((i*i)); done' \
    "$squares_output"
expect_output "Even parity detection using arithmetic test" \
    'value=4; if [ $((value % 2)) -ne 0 ]; then echo odd; else echo even; fi' \
    "even"
expect_output "Odd parity detection using arithmetic test" \
    'value=5; if [ $((value % 2)) -ne 0 ]; then echo odd; else echo even; fi' \
    "odd"
comparison_output=$(printf "3\n2\n1\n")
expect_output "Chained decrements inside arithmetic pipeline" \
    'count=3; while [ $count -gt 0 ]; do echo $count; count=$((count-1)); done' \
    "$comparison_output"
printf "\nArithmetic comprehensive test summary: %d passed, %d failed, %d total\n" \
    "$PASSED" "$FAILED" "$TOTAL"
if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
