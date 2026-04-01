#!/usr/bin/env sh

# test_arithmetic_command_form_and_c_style_for.sh
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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
    rc=$?
    if [ "$rc" -eq "$expected_exit" ]; then
        pass "$desc"
    else
        fail "$desc" "expected exit $expected_exit, got $rc"
    fi
}

expect_nonzero_exit() {
    desc=$1
    script=$2
    TOTAL=$((TOTAL + 1))
    "$CJSH_PATH" -c "$script" >/dev/null 2>&1
    rc=$?
    if [ "$rc" -ne 0 ]; then
        pass "$desc"
    else
        fail "$desc" "expected non-zero exit, got $rc"
    fi
}

expect_output "((1)) returns success status" \
    '((1)); echo $?' \
    "0"

expect_output "((0)) returns failure status" \
    '((0)); echo $?' \
    "1"

expect_output "((2-2)) returns failure status" \
    '((2-2)); echo $?' \
    "1"

expect_output "! ((0)) inverts to success" \
    '! ((0)); echo $?' \
    "0"

expect_output "! ((4)) inverts to failure" \
    '! ((4)); echo $?' \
    "1"

expect_output "Post increment updates var and status" \
    'a=0; ((a++)); rc=$?; echo "$a $rc"' \
    "1 1"

expect_output "Pre increment updates var and status" \
    'a=0; ((++a)); rc=$?; echo "$a $rc"' \
    "1 0"

expect_output "Post decrement updates var and status" \
    'a=1; ((a--)); rc=$?; echo "$a $rc"' \
    "0 0"

expect_output "Pre decrement to zero sets failure status" \
    'a=1; ((--a)); rc=$?; echo "$a $rc"' \
    "0 1"

expect_output "Compound += in (( ))" \
    'a=2; ((a+=3)); rc=$?; echo "$a $rc"' \
    "5 0"

expect_output "Compound -= to zero in (( ))" \
    'a=5; ((a-=5)); rc=$?; echo "$a $rc"' \
    "0 1"

expect_output "Assignment in (( )) writes variable" \
    'a=0; ((a=7)); rc=$?; echo "$a $rc"' \
    "7 0"

expect_output "Assignment expression in (( )) updates variable" \
    'a=0; ((a=a+1)); rc=$?; echo "$a $rc"' \
    "1 0"

expect_output "Assignment used in comparison in (( )) updates variable" \
    'a=0; (( (a=a+1) < 3 )); rc=$?; echo "$a $rc"' \
    "1 0"

expect_output "Logical AND in (( ))" \
    'a=1; b=0; ((a&&b)); echo $?' \
    "1"

expect_output "Logical OR in (( ))" \
    'a=1; b=0; ((a||b)); echo $?' \
    "0"

expect_output "Nested \$(( )) inside (( ))" \
    'a=1; ((a += $((2+3)))); echo $a' \
    "6"

expect_output "((0)) works with || list operator" \
    '((0)) || echo fallback' \
    "fallback"

expect_output "((1)) works with && list operator" \
    '((1)) && echo ok' \
    "ok"

expect_output "(( )) works in if true branch" \
    'v=3; if ((v)); then echo yes; else echo no; fi' \
    "yes"

expect_output "(( )) works in if false branch" \
    'v=0; if ((v)); then echo yes; else echo no; fi' \
    "no"

expect_output "(( )) works in while condition" \
    'i=0
out=""
while ((i<3))
do
    out="${out}${i}"
    ((i++))
done
echo "$out"' \
    "012"

expect_output "(( )) works in until condition" \
    'i=0; out=""; until ((i==3)); do out="${out}${i}"; ((i++)); done; echo "$out"' \
    "012"

expect_output "Whitespace-insensitive arithmetic command" \
    'a=2; (( a + 3 )); echo $?' \
    "0"

expect_output "Parenthesized arithmetic command expression" \
    '(( (2+3) * 2 )); echo $?' \
    "0"

expect_nonzero_exit "Division by zero in (( )) fails" \
    '((5/0))'

expect_output "Substitution-fed variable in (( )) expression" \
    'x=$(printf 2); ((x += 1)); rc=$?; echo "$x $rc"' \
    "3 0"

expect_output "Unset var increments from zero in (( ))" \
    'unset z; ((z++)); rc=$?; echo "$z $rc"' \
    "1 1"

expect_output "Underscore variable in (( ))" \
    '_a=4; ((_a++)); echo $_a' \
    "5"

expect_output "C-style for prints sequence" \
    'for ((i=0; i<3; i++)); do echo $i; done' \
    "$(printf "0\n1\n2")"

expect_output "C-style for accumulation" \
    'sum=0; for ((i=1; i<=4; i++)); do sum=$((sum+i)); done; echo $sum' \
    "10"

expect_output "C-style for with empty init" \
    'i=1; sum=0; for ((; i<=3; i++)); do sum=$((sum+i)); done; echo $sum' \
    "6"

expect_output "C-style for with empty condition" \
    'for ((i=0; ; i++))
do
    if ((i==3)); then
        break
    fi
    :
done
echo $i' \
    "3"

expect_output "C-style for with empty update" \
    'i=0; count=0; for ((i=0; i<3; )); do count=$((count+1)); ((i++)); done; echo "$i $count"' \
    "3 3"

expect_output "C-style for decrementing loop" \
    'out=""; for ((i=3; i>0; i--)); do out="${out}${i}"; done; echo "$out"' \
    "321"

expect_output "C-style for with += update" \
    'out=""; for ((i=0; i<6; i+=2)); do out="${out}${i}"; done; echo "$out"' \
    "024"

expect_output "C-style for with ++i update" \
    'out=""; for ((i=0; i<3; ++i)); do out="${out}${i}"; done; echo "$out"' \
    "012"

expect_output "C-style for one-liner executes trailing command" \
    'for ((i=0; i<1; i++)); do :; done; echo $i' \
    "1"

expect_output "C-style for logical condition" \
    'out=""; for ((i=0; i<6 && i%2==0; i+=2)); do out="${out}${i}"; done; echo "$out"' \
    "024"

expect_output "C-style for final loop var persists" \
    'for ((i=0; i<3; i++))
do
    :
done
echo $i' \
    "3"

expect_output "C-style for zero iterations leaves init value" \
    'i=7; for ((i=7; i<3; i++)); do echo bad; done; echo $i' \
    "7"

expect_output "C-style for continue still applies update" \
    'sum=0; for ((i=0; i<5; i++)); do if ((i==2)); then continue; fi; sum=$((sum+i)); done; echo "$sum $i"' \
    "8 5"

expect_output "C-style for break stops before update" \
    'for ((i=0; i<10; i++))
do
    if ((i==4)); then
        break
    fi
    :
done
echo $i' \
    "4"

expect_output "Nested C-style loops" \
    'count=0; for ((i=0; i<3; i++)); do for ((j=0; j<2; j++)); do count=$((count+1)); done; done; echo $count' \
    "6"

expect_output "Inline C-style for body" \
    'sum=0; for ((i=1; i<=3; i++)); do sum=$((sum+i)); done; echo $sum' \
    "6"

expect_output "C-style for inside if block" \
    'if ((1))
then
    for ((i=0; i<2; i++))
    do
        echo $i
    done
fi' \
    "$(printf "0\n1")"

expect_output "While one-liner executes trailing command" \
    'while ((1)); do break; done; echo tail' \
    "tail"

expect_output "Until one-liner executes trailing command" \
    'until ((1)); do :; done; echo tail' \
    "tail"

expect_output "Arithmetic command in C-style body" \
    'for ((i=0; i<3; i++)); do ((i+=0)); echo $i; done' \
    "$(printf "0\n1\n2")"

expect_output "Classic for-in still works" \
    'out=""; for x in a b c; do out="${out}${x}"; done; echo "$out"' \
    "abc"

expect_output "Brace range for-in still works" \
    'out=""; for x in {1..3}; do out="${out}${x}"; done; echo "$out"' \
    "123"

expect_output "C-style for condition with nested parens" \
    'count=0; for ((i=0; (i+1)<=3; i++)); do count=$((count+1)); done; echo $count' \
    "3"

expect_output "C-style for condition with external limit" \
    'limit=3
hits=0
for ((i=0; i<limit; i++))
do
    hits=$((hits+1))
done
echo "$i $hits"' \
    "3 3"

expect_output "C-style for condition with assignment" \
    'hits=0
for ((i=0; (i=i+1)<=3; ))
do
    hits=$((hits+1))
done
echo "$i $hits"' \
    "4 3"

expect_output "C-style for update using variable step" \
    'step=2; out=""; for ((i=0; i<5; i+=step)); do out="${out}${i}"; done; echo "$out"' \
    "024"

expect_exit "Missing do in C-style for reports syntax failure" \
    'for ((i=0; i<3; i++)); echo hi' \
    2

printf "\nArithmetic command + C-style for summary: %d passed, %d failed, %d total\n" \
    "$PASSED" "$FAILED" "$TOTAL"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
