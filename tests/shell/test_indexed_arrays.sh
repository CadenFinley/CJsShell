#!/usr/bin/env sh

# test_indexed_arrays.sh
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

expect_output "Indexed assignment stores sparse values" \
    'arr[0]=first; arr[2]=third; printf "%s|%s|%s" "${arr[0]}" "${arr[2]}" "${arr[1]}"' \
    "first|third|"

expect_output "Array literal assigns sequential indexes" \
    'arr=(alpha beta gamma); printf "%s|%s|%s" "${arr[0]}" "${arr[1]}" "${arr[2]}"' \
    "alpha|beta|gamma"

expect_output "Indexed entries in literals preserve sparse indexes" \
    'arr=([2]=c one [5]=f); printf "%s|%s|%s|%s" "${arr[2]}" "${arr[3]}" "${arr[5]}" "${arr[4]}"' \
    "c|one|f|"

expect_output "Array append operators follow bash style" \
    'arr=(a b); arr+=(c d); arr[1]+=X; printf "%s|%s|%s|%s" "${arr[0]}" "${arr[1]}" "${arr[2]}" "${arr[3]}"' \
    "a|bX|c|d"

expect_output "Array count and index expansion work" \
    'arr=([1]=one [3]=three); printf "%s|%s" "${#arr[@]}" "${!arr[@]}"' \
    "2|1 3"

expect_output "Scalar variables support array-style indexing" \
    'name=hello; printf "%s|%s|%s|%s" "${name[0]}" "${#name[@]}" "${!name[@]}" "${name[@]}"' \
    "hello|1|0|hello"

expect_output "Sparse arrays keep plain expansion empty" \
    'arr[2]=x; printf "%s|%s|%s" "${arr}" "${arr[2]}" "${arr[@]}"' \
    "|x|x"

expect_output "unset removes indexed elements and whole arrays" \
    'arr=(a b c); unset "arr[1]"; printf "%s|%s|%s|%s" "${#arr[@]}" "${arr[1]}" "${arr[2]}" "${!arr[@]}"; unset arr; printf "|%s|%s" "${arr[0]}" "${#arr[@]}"' \
    "2||c|0 2||0"

expect_exit "Negative indexes fail assignment" \
    'arr[-1]=x' \
    1

printf "\nIndexed array summary: %d passed, %d failed, %d total\n" "$PASSED" "$FAILED" "$TOTAL"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
