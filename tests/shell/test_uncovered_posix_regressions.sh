#!/usr/bin/env sh

# test_uncovered_posix_regressions.sh
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

echo "Test: uncovered POSIX regressions..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    printf 'PASS: %s\n' "$1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    printf 'FAIL: %s\n' "$1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

expect_output() {
    description=$1
    script=$2
    expected=$3

    output=$("$CJSH_PATH" --secure -c "$script" 2>&1)
    status=$?
    if [ "$status" -eq 0 ] && [ "$output" = "$expected" ]; then
        pass_test "$description"
    else
        fail_test "$description (status=$status, expected='$expected', got='$output')"
    fi
}

expect_output "printf \\c stops only the current printf builtin" \
    'printf "%b" "x\c"; echo AFTER' \
    'xAFTER'

expect_output "function invocation honors stdout redirection" \
    'f(){ echo leaked; }; f >/dev/null; echo after' \
    'after'

expect_output "errexit ignores a failed non-final AND-list command" \
    'set -e; false && echo bad; echo after' \
    'after'

expect_output "assignment-only command preserves substitution status" \
    'x=$(exit 7); printf "%s" "$?"' \
    '7'

expect_output "assignment before a special builtin persists" \
    'x=old; x=new export y=z; printf "%s" "$x"' \
    'new'

expect_output "assignment prefix dispatches a shell function" \
    'f(){ printf "<%s>" "$x"; }; x=new f' \
    '<new>'

expect_output "non-whitespace IFS delimiters preserve interior empty fields" \
    'IFS=:; value="a::b"; set -- $value; printf "%s:%s:%s:%s" "$#" "$1" "$2" "$3"' \
    '3:a::b'

expect_output "return without an operand preserves the previous status" \
    'f(){ false; return; }; f; printf "%s" "$?"' \
    '1'

expect_output "for without in iterates over positional parameters" \
    'set -- "a b" c; for value; do printf "<%s>" "$value"; done' \
    '<a b><c>'

expect_output "quoted dollar-at preserves prefix and suffix placement" \
    'set -- a b; printf "<%s>\n" "x$@y"' \
    '<xa>
<by>'

expect_output "multi-digit positional parameters expand correctly" \
    'set -- 0 1 2 3 4 5 6 7 8 9; printf "<%s:%s>" "$10" "${10}"' \
    '<00:9>'

expect_output "arithmetic logical operators short-circuit side effects" \
    'x=0; : $((0 && (x=1))); printf "<%s>" "$x"' \
    '<0>'

expect_output "quoted case metacharacters remain literal" \
    'value=abc; case $value in "*") echo bad;; *) echo good;; esac' \
    'good'

expect_output "brace group works on the right side of a pipeline" \
    'printf "yes\n" | { read value; echo "got:$value"; }' \
    'got:yes'

echo ""
echo "Uncovered POSIX Regression Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ "$TESTS_FAILED" -eq 0 ]; then
    echo "PASS"
    exit 0
fi

echo "FAIL"
exit 1
