#!/usr/bin/env sh

# test_indexed_arrays_parameter_expansion.sh
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

expect_output "Element default operators on unset index" \
    'unset arr; echo "${arr[0]:-d}|${arr[0]-d}"' \
    "d|d"

expect_output "Element default operators on empty value" \
    'arr[0]=""; echo "${arr[0]:-d}|${arr[0]-d}"' \
    "d|"

expect_output "Element default operators on non-empty value" \
    'arr[0]=x; echo "${arr[0]:-d}|${arr[0]-d}"' \
    "x|x"

expect_output "Element plus operators on unset index" \
    'unset arr; echo "x${arr[0]+Y}x|x${arr[0]:+Y}x"' \
    "xx|xx"

expect_output "Element plus operators on empty value" \
    'arr[0]=""; echo "x${arr[0]+Y}x|x${arr[0]:+Y}x"' \
    "xYx|xx"

expect_output "Element plus operators on non-empty value" \
    'arr[0]=x; echo "x${arr[0]+Y}x|x${arr[0]:+Y}x"' \
    "xYx|xYx"

expect_output "Array join plus operators on unset array" \
    'unset arr; echo "x${arr[@]+Y}x|x${arr[@]:+Y}x"' \
    "xx|xx"

expect_output "Array join plus operators on non-empty array" \
    'arr[2]=x; echo "x${arr[@]+Y}x|x${arr[@]:+Y}x"' \
    "xYx|xYx"

expect_output "Array join plus operators on empty element array" \
    'arr[2]=""; echo "x${arr[@]+Y}x|x${arr[@]:+Y}x"' \
    "xYx|xx"

expect_output "Element := assigns missing indexed value" \
    'unset arr; echo ${arr[1]:=hi}; echo "${arr[1]}|${#arr[@]}|${!arr[@]}"' \
    "hi
hi|1|1"

expect_output "Array count and key expansions support sparse arrays" \
    'arr=([1]=one [3]=three); echo "${#arr[@]}|${#arr[*]}|${!arr[@]}|${!arr[*]}"' \
    "2|2|1 3|1 3"

expect_output "Scalar variables remain compatible with array forms" \
    'name=hello; echo "${name[0]}|${#name[0]}|${#name}|${#name[@]}|${!name[@]}"' \
    "hello|5|5|1|0"

expect_output "Pattern and case operators work on indexed elements" \
    'arr[1]=hello_world; echo "${arr[1]#he}|${arr[1]%_world}|${arr[1]/_/ }|${arr[1]^^}|${arr[1],,}"' \
    "llo_world|hello|hello world|HELLO_WORLD|hello_world"

expect_output "Substring operators work on indexed elements" \
    'arr[1]=x; echo "${#arr[1]}|${arr[1]:1}|${arr[1]:0:1}"' \
    "1||x"

expect_output "Sparse arrays preserve ordered keys during expansion" \
    'arr=(a b c); unset arr[1]; echo "${!arr[@]}|${arr[@]}"' \
    "0 2|a c"

expect_output "Scalar star variants mirror scalar array compatibility" \
    'name=hello; echo "${#name[*]}|${!name[*]}"' \
    "1|0"

printf "\nIndexed parameter expansion summary: %d passed, %d failed, %d total\n" \
    "$PASSED" "$FAILED" "$TOTAL"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
