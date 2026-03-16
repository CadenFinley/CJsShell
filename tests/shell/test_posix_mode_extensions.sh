#!/usr/bin/env sh
# test_posix_mode_extensions.sh
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


TOTAL=0
PASSED=0
FAILED=0

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
DEFAULT_SHELL="$SCRIPT_DIR/../../build/cjsh"

if [ -n "$1" ]; then
    SHELL_TO_TEST="$1"
elif [ -z "$SHELL_TO_TEST" ]; then
    if [ -n "$CJSH" ]; then
        SHELL_TO_TEST="$CJSH"
    else
        SHELL_TO_TEST="$DEFAULT_SHELL"
    fi
fi

if [ "${SHELL_TO_TEST#/}" = "$SHELL_TO_TEST" ]; then
    SHELL_TO_TEST="$(pwd)/$SHELL_TO_TEST"
fi

TEST_HOME=$(mktemp -d "${TMPDIR:-/tmp}/cjsh-posix-mode-extensions.XXXXXX")
if [ -z "$TEST_HOME" ] || [ ! -d "$TEST_HOME" ]; then
    echo "Failed to create temporary HOME for tests"
    exit 1
fi
trap 'rm -rf "$TEST_HOME"' EXIT INT TERM
export HOME="$TEST_HOME"
unset CJSH_ENV


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

echo "Checking interactive behavior without POSIX gating for: $SHELL_TO_TEST"
echo "======================================================="

log_test "--posix flag accepted"
output=$("$SHELL_TO_TEST" --posix -c 'echo hi' 2>&1)
status=$?
if [ $status -eq 0 ] && [ "$output" = "hi" ]; then
    pass
else
    clean_output=$(printf "%s" "$output" | tr '\n' ' ')
    fail "Expected status 0 and 'hi' output (status=$status, output=$clean_output)"
fi

log_test "Brace expansion active"
result=$("$SHELL_TO_TEST" -c 'echo {1..3}' 2>/dev/null)
if [ "$result" = "1 2 3" ]; then
    pass
else
    fail "Expected '1 2 3', got '$result'"
fi

log_test "Double bracket builtin available"
"$SHELL_TO_TEST" -c '[[ 1 == 1 ]]' >/dev/null 2>&1
status=$?
if [ $status -eq 0 ]; then
    pass
else
    fail "[[ ]] should succeed (status=$status)"
fi

log_test "cjshopt available"
output=$("$SHELL_TO_TEST" -c 'cjshopt completion-case status' 2>&1)
status=$?
if [ $status -eq 0 ]; then
    pass
else
    clean_output=$(printf "%s" "$output" | tr '\n' ' ')
    fail "cjshopt command failed (status=$status, output=$clean_output)"
fi

log_test "Here-strings supported"
output=$("$SHELL_TO_TEST" -c 'cat <<< "posix"' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -eq 0 ] && [ "$clean_output" = "posix" ]; then
    pass
else
    fail "Expected here-string to output 'posix' (status=$status, output=$clean_output)"
fi

log_test "Process substitution available"
output=$("$SHELL_TO_TEST" -c 'cat <(echo posix)' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -eq 0 ] && [ "$clean_output" = "posix" ]; then
    pass
else
    fail "Expected process substitution to output 'posix' (status=$status, output=$clean_output)"
fi

log_test "function keyword allowed"
output=$("$SHELL_TO_TEST" -c 'function foo { echo hi; }; foo' 2>&1)
status=$?
clean_output=$(printf "%s" "$output" | tr '\n' ' ')
if [ $status -eq 0 ] && [ "$clean_output" = "hi" ]; then
    pass
else
    fail "Expected function keyword to execute (status=$status, output=$clean_output)"
fi

echo "======================================================="
echo "Interactive Mode Tests Summary:"
echo "Total: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
