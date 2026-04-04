#!/usr/bin/env sh

# test_declare_builtin.sh
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

echo "Test: declare/typeset builtin behavior..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

run_expect_output() {
    desc=$1
    cmd=$2
    expected=$3

    out=$("$CJSH_PATH" -c "$cmd" 2>/dev/null)
    status=$?

    if [ "$status" -eq 0 ] && [ "$out" = "$expected" ]; then
        pass_test "$desc"
    else
        fail_test "$desc (status=$status, out=$out, expected=$expected)"
    fi
}

run_expect_fail() {
    desc=$1
    cmd=$2
    pattern=$3

    out=$("$CJSH_PATH" -c "$cmd" 2>&1)
    status=$?

    if [ "$status" -ne 0 ] && printf "%s" "$out" | grep -Fq -- "$pattern"; then
        pass_test "$desc"
    else
        clean_out=$(printf "%s" "$out" | tr '\n' ' ')
        fail_test "$desc (status=$status, out=$clean_out)"
    fi
}

run_expect_contains() {
    desc=$1
    cmd=$2
    pattern=$3

    out=$("$CJSH_PATH" -c "$cmd" 2>&1)
    status=$?

    if [ "$status" -eq 0 ] && printf "%s" "$out" | grep -Fq -- "$pattern"; then
        pass_test "$desc"
    else
        clean_out=$(printf "%s" "$out" | tr '\n' ' ')
        fail_test "$desc (status=$status, out=$clean_out)"
    fi
}

run_expect_output "declare sets scalar values" 'declare VAR_DECL=hello; printf "%s" "$VAR_DECL"' "hello"

run_expect_output "typeset aliases declare" 'typeset VAR_TYPESET=works; printf "%s" "$VAR_TYPESET"' "works"

run_expect_output "declare defaults to local scope inside functions" \
    'VALUE=outer; f() { declare VALUE=inner; echo "$VALUE"; }; f; echo "$VALUE"' \
    "inner
outer"

run_expect_output "declare -g updates global scope inside functions" \
    'VALUE=outer; f() { declare -g VALUE=inner; }; f; printf "%s" "$VALUE"' \
    "inner"

run_expect_output "declare -x exports values to child processes" \
    "declare -x DECLARE_EXPORT=ready; /bin/sh -c 'printf \"%s\" \"\$DECLARE_EXPORT\"'" \
    "ready"

run_expect_output "declare +x removes export while keeping shell value" \
    "declare -x DECLARE_KEEP=value; declare +x DECLARE_KEEP; printf '%s|' \"\$DECLARE_KEEP\"; /bin/sh -c 'printf \"%s\" \"\${DECLARE_KEEP:-unset}\"'" \
    "value|unset"

run_expect_fail "declare -r enforces readonly assignments" \
    "declare -r DECLARE_RO=one; DECLARE_RO=two" \
    "readonly variable"

run_expect_output "declare -a creates indexed arrays" \
    "declare -a ARR_DECL=(zero one two); printf '%s' \"\${ARR_DECL[1]}\"" \
    "one"

run_expect_output "declare -a scalar assignment populates index zero" \
    "declare -a ARR_SCALAR=alpha; printf '%s' \"\${ARR_SCALAR[0]}\"" \
    "alpha"

run_expect_output "declare -a supports indexed assignment" \
    "declare -a ARR_INDEXED; ARR_INDEXED[3]=three; printf '%s' \"\${ARR_INDEXED[3]}\"" \
    "three"

run_expect_contains "declare -p prints declaration format" \
    "declare PRINT_ME=value; declare -p PRINT_ME" \
    "declare"

run_expect_contains "declare -f lists function declarations" \
    "f_declare_builtin() { :; }; declare -f f_declare_builtin" \
    "declare -f f_declare_builtin"

run_expect_output "declare -fr marks functions readonly" \
    "f_ro() { echo one; }; declare -fr f_ro; f_ro() { echo two; } 2>/dev/null; code=\$?; f_ro; printf 'code:%s' \"\$code\"" \
    "one
code:1"

echo ""
echo "Declare Builtin Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
