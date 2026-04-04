#!/usr/bin/env sh

# test_indexed_arrays_scope_and_unset.sh
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

expect_script_output() {
    desc=$1
    expected=$2
    script_content=$3
    TOTAL=$((TOTAL + 1))

    tmp_script=$(mktemp)
    printf "%s\n" "$script_content" >"$tmp_script"
    output=$("$CJSH_PATH" "$tmp_script" 2>&1)
    rm -f "$tmp_script"

    if [ "$output" = "$expected" ]; then
        pass "$desc"
    else
        fail "$desc" "expected: [$expected], got: [$output]"
    fi
}

expect_output "unset removes single indexed element" \
    'arr=(a b c); unset arr[1]; echo "${#arr[@]}|${!arr[@]}|${arr[@]}"' \
    "2|0 2|a c"

expect_output "unset arr[@] clears entire indexed array" \
    'arr=(a b); unset arr[@]; echo "${#arr[@]}|${arr[@]}|${!arr[@]}"' \
    "0||"

expect_output "unset arr[*] clears entire indexed array" \
    'arr=(a b); unset arr[*]; echo "${#arr[@]}|${arr[@]}|${!arr[@]}"' \
    "0||"

expect_output "unset scalar[@] removes scalar bindings" \
    'v=hello; unset v[@]; echo "$?|x${v}x|${#v[@]}|${!v[@]}"' \
    "0|xx|0|"

expect_output "unset scalar[*] removes scalar bindings" \
    'v=hello; unset v[*]; echo "$?|x${v}x|${#v[@]}|${!v[@]}"' \
    "0|xx|0|"

expect_output "unset scalar[0] is a no-op for scalar compatibility" \
    'v=hello; unset v[0]; echo "$?|x${v}x|${#v[@]}|${!v[@]}"' \
    "0|xhellox|1|0"

expect_output "unset supports arithmetic index expressions" \
    'arr=(a b c); unset arr[1+1]; echo "${#arr[@]}|${!arr[@]}|${arr[@]}"' \
    "2|0 1|a b"

expect_output "unset indexed target on missing array is a no-op" \
    'unset arr[1]; echo "$?"' \
    "0"

expect_exit "readonly blocks indexed unset targets" \
    'readonly arr=foo; unset arr[0]' \
    1

expect_exit "readonly blocks indexed assignment targets" \
    'readonly arr=foo; arr[0]=bar' \
    1

expect_exit "readonly blocks array append targets" \
    'readonly arr=(a b); arr+=(c)' \
    1

expect_script_output "Local indexed arrays shadow globals in function scope" \
"in:l0:lx:2
out:g0:g1:2" \
'arr=(g0 g1)
f() {
    local arr=(l0 l1)
    arr[1]=lx
    echo "in:${arr[0]}:${arr[1]}:${#arr[@]}"
}
f
echo "out:${arr[0]}:${arr[1]}:${#arr[@]}"'

expect_script_output "Local array append declarations preserve existing locals" \
"a:b:c:d" \
'f() {
    local arr=(a b)
    local arr+=(c d)
    echo "${arr[0]}:${arr[1]}:${arr[2]}:${arr[3]}"
}
f'

expect_script_output "Local indexed declaration initializes sparse local arrays" \
":x:1:1" \
'f() {
    local arr[1]=x
    echo "${arr[0]}:${arr[1]}:${#arr[@]}:${!arr[@]}"
}
f'

expect_script_output "unset works on local indexed arrays without global leakage" \
"2:0 2:a c" \
'f() {
    local arr=(a b c)
    unset arr[1]
    echo "${#arr[@]}:${!arr[@]}:${arr[@]}"
}
f'

expect_script_output "unsetting local array name reveals global binding" \
"in:2
out:g0:g1:2" \
'arr=(g0 g1)
f() {
    local arr=(l0 l1)
    unset arr
    echo "in:${#arr[@]}"
}
f
echo "out:${arr[0]}:${arr[1]}:${#arr[@]}"'

expect_script_output "unsetting local indexed element does not alter global array" \
"in:l0 l2:0 2
out:g0 g1 g2:0 1 2" \
'arr=(g0 g1 g2)
f() {
    local arr=(l0 l1 l2)
    unset arr[1]
    echo "in:${arr[@]}:${!arr[@]}"
}
f
echo "out:${arr[@]}:${!arr[@]}"'

expect_script_output "export of local indexed array exposes element zero to child" \
"x" \
'f() {
    local arr=(x y)
    export arr
    sh -c "printf %s \"$arr\""
}
f'

expect_script_output "exported local indexed array does not persist after function" \
"g0:g1:2" \
'arr=(g0 g1)
f() {
    local arr=(x y)
    export arr
}
f
echo "${arr[0]}:${arr[1]}:${#arr[@]}"'

expect_exit "export rejects indexed array operands" \
    'arr=(a b); export arr[1]' \
    1

printf "\nIndexed scope/unset summary: %d passed, %d failed, %d total\n" \
    "$PASSED" "$FAILED" "$TOTAL"

if [ "$FAILED" -eq 0 ]; then
    exit 0
else
    exit 1
fi
