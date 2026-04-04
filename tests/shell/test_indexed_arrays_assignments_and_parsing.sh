#!/usr/bin/env sh

# test_indexed_arrays_assignments_and_parsing.sh
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

expect_output "Arithmetic indexes resolve for assignment targets" \
    'i=1; arr[i+2]=x; arr[1+1]=y; echo "${arr[3]}|${arr[2]}|${!arr[@]}"' \
    "x|y|2 3"

expect_output "Sparse literal assignment and retrieval works" \
    'arr=([2]=c one [5]=f [5]+=X); echo "${arr[2]}|${arr[3]}|${arr[5]}|${#arr[@]}"' \
    "c|one|fX|3"

expect_output "Array literal append supports indexed and implicit elements" \
    'arr=(a b); arr+=([5]=x y); echo "${arr[0]}|${arr[1]}|${arr[5]}|${arr[6]}|${#arr[@]}|${!arr[@]}"' \
    "a|b|x|y|4|0 1 5 6"

expect_output "Array append cursor tracks mixed explicit and implicit indexes" \
    'arr=([2]=c); arr+=(x [5]=y z); echo "${arr[2]}|${arr[3]}|${arr[5]}|${arr[6]}|${#arr[@]}|${!arr[@]}"' \
    "c|x|y|z|4|2 3 5 6"

expect_output "Scalar-to-array conversion preserves element zero" \
    'v=hello; v[2]=x; echo "$v|${v[0]}|${v[2]}|${#v[@]}|${!v[@]}"' \
    "hello|hello|x|2|0 2"

expect_output "Scalar literal append conversion keeps original element zero" \
    'v=hello; v+=(x y); echo "${v[0]}|${v[1]}|${v[2]}|${#v[@]}|${!v[@]}"' \
    "hello|x|y|3|0 1 2"

expect_output "Scalar assignment updates element zero without clearing sparse members" \
    'arr=(a b c); arr=z; echo "${arr[0]}|${arr[1]}|${arr[2]}|${#arr[@]}|${!arr[@]}|$arr"' \
    "z|b|c|3|0 1 2|z"

expect_output "Base += appends to element zero for indexed arrays" \
    'arr=(a b); arr+=z; echo "${arr[0]}|${arr[1]}|${#arr[@]}"' \
    "az|b|2"

expect_output "Indexed += creates new sparse element when missing" \
    'arr=(a b); arr[5]+=x; echo "${arr[5]}|${#arr[@]}|${!arr[@]}"' \
    "x|3|0 1 5"

expect_output "Empty array literal resets previous indexed values" \
    'arr=(a b); arr=(); echo "${#arr[@]}|${arr[0]}|${arr[@]}"' \
    "0||"

expect_exit "Negative indexes are rejected" \
    'arr[-1]=x' \
    1

expect_exit "Join indexes are rejected for direct assignment targets" \
    'arr[@]=x' \
    1

expect_exit "Join indexes are rejected for direct star targets" \
    'arr[*]=x' \
    1

expect_exit "Join indexes are rejected in explicit literal elements" \
    'arr=([@]=x)' \
    1

expect_output "Indexed assignment preserves substitution exit status" \
    'arr[0]=$(printf ok); echo "$?|${arr[0]}"' \
    "0|ok"

expect_output "Failed substitution still records indexed element assignment" \
    'arr[0]=$(false); echo "$?|${#arr[@]}|x${arr[0]}x"' \
    "1|1|xx"

expect_output "Failed substitution in literal assignment returns failure" \
    'arr=($(false)); echo "$?|${#arr[@]}"' \
    "1|0"

expect_output "Brace expansion is suppressed for scalar assignment tokens" \
    'v={a,b}; echo "$v"' \
    "{a,b}"

expect_output "Brace expansion is suppressed for indexed assignment targets" \
    'arr[0]={c,d}; echo "${arr[0]}"' \
    "{c,d}"

expect_output "Brace expansion still applies to plain literal words" \
    'arr=({x,y}); echo "${arr[0]}|${arr[1]}|${#arr[@]}"' \
    "x|y|2"

expect_output "Indexed literal tokens suppress brace expansion per-element" \
    'arr=([2]={p,q} {m,n}); echo "${arr[2]}|${arr[3]}|${arr[4]}|${#arr[@]}"' \
    "{p,q}|m|n|3"

expect_output "Wildcard expansion is suppressed in indexed assignment tokens" \
    'tmp=$(mktemp -d); cd "$tmp"; touch sample.c; arr[0]=*.c; echo "${arr[0]}"; rm -rf "$tmp"' \
    "*.c"

expect_output "Wildcard expansion applies to plain literal words" \
    'tmp=$(mktemp -d); cd "$tmp"; touch sample.c; arr=(*.c); echo "${arr[0]}|${#arr[@]}"; rm -rf "$tmp"' \
    "sample.c|1"

expect_output "Indexed literal wildcard tokens remain literal while plain words expand" \
    'tmp=$(mktemp -d); cd "$tmp"; touch sample.c; arr=([2]=*.c *.c); echo "${arr[2]}|${arr[3]}|${#arr[@]}"; rm -rf "$tmp"' \
    "*.c|sample.c|2"

printf "\nIndexed assignment/parsing summary: %d passed, %d failed, %d total\n" \
    "$PASSED" "$FAILED" "$TOTAL"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
