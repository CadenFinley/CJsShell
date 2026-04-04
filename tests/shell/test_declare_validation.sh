#!/usr/bin/env sh

# test_declare_validation.sh
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

echo "Test: declare/typeset validation and diagnostics..."

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

run_expect_posix_fail() {
    desc=$1
    cmd=$2
    pattern=$3

    out=$("$CJSH_PATH" --posix -c "$cmd" 2>&1)
    status=$?

    if [ "$status" -ne 0 ] && printf "%s" "$out" | grep -Fq -- "$pattern"; then
        pass_test "$desc"
    else
        clean_out=$(printf "%s" "$out" | tr '\n' ' ')
        fail_test "$desc (status=$status, out=$clean_out)"
    fi
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

run_expect_output_contains() {
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

run_expect_fail "declare rejects unknown option" "declare -z var=1" "invalid option: -z"
run_expect_fail "typeset rejects unknown option" "typeset -z var=1" "invalid option: -z"
run_expect_fail "declare rejects +g" "declare +g var" "invalid option: +g"
run_expect_fail "declare cannot unset readonly attribute" "declare +r var" "cannot unset readonly attribute"
run_expect_fail "declare cannot unset array attribute" "declare +a arr" "cannot unset array attribute"
run_expect_fail "declare function mode rejects -x combination" "declare -fx fn" "incompatible option combination for function mode"
run_expect_fail "declare function mode rejects -g combination" "declare -fg fn" "incompatible option combination for function mode"

run_expect_fail "declare rejects invalid assignment identifier" "declare 1bad=value" "invalid variable name"
run_expect_fail "declare rejects invalid bare identifier" "declare bad-name" "invalid variable name"
run_expect_fail "declare rejects indexed target without assignment" "declare arr[1]" "invalid variable name"
run_expect_fail "declare -p rejects invalid identifier" "declare -p bad-name" "invalid variable name"
run_expect_fail "declare -p reports missing variable" "declare -p DOES_NOT_EXIST_123" "not found"

run_expect_fail "declare rejects malformed array literal" "declare -a arr=( one two" "unclosed array assignment"
run_expect_fail "declare rejects invalid array literal index" "declare -a arr= ( [@]=1 )" "invalid assignment target"

run_expect_fail "declare -f reports missing function" "declare -f missing_function_123" "function not found"
run_expect_fail "declare -F reports missing function" "declare -F missing_function_123" "function not found"

run_expect_posix_fail "declare blocked by POSIX validator" "declare posix_var=1" "POSIX011"
run_expect_posix_fail "typeset blocked by POSIX validator" "typeset posix_var=1" "POSIX011"
run_expect_posix_fail "declare blocked at runtime in compound command" "if true; then declare posix_rt=1; fi" "'declare' is disabled in POSIX mode"
run_expect_posix_fail "typeset blocked at runtime in compound command" "if true; then typeset posix_rt=1; fi" "'typeset' is disabled in POSIX mode"

run_expect_output "declare assigns scalar values" 'declare scalar_ok=ready; printf "%s" "$scalar_ok"' "ready"
run_expect_output "typeset aliases declare assignment" 'typeset alias_ok=works; printf "%s" "$alias_ok"' "works"
run_expect_output_contains "declare -f prints known functions" "f_decl_test() { :; }; declare -f f_decl_test" "declare -f f_decl_test"

echo ""
echo "Declare Validation Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
