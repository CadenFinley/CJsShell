#!/usr/bin/env sh

# test_approot.sh
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

echo "Test: approot builtin..."

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

run_path_test() {
    description=$1
    command_text=$2
    expected_path=$3

    output=$("$CJSH_PATH" -c "$command_text")
    status=$?

    if [ "$status" -eq 0 ] && [ "$output" = "$expected_path" ]; then
        pass_test "$description"
    else
        fail_test "$description (status=$status, expected '$expected_path', got '$output')"
    fi
}

CONFIG_DIR="$HOME/.config/cjsh"
CACHE_DIR="$HOME/.cache/cjsh"
COMPLETIONS_DIR="$CACHE_DIR/generated_completions"

mkdir -p "$CONFIG_DIR" "$COMPLETIONS_DIR"

CONFIG_EXPECTED=$(cd "$CONFIG_DIR" && pwd -P)
CACHE_EXPECTED=$(cd "$CACHE_DIR" && pwd -P)
COMPLETIONS_EXPECTED=$(cd "$COMPLETIONS_DIR" && pwd -P)
HOME_EXPECTED=$(cd "$HOME" && pwd -P)
CJSH_EXPECTED=$(cd "$(dirname "$CJSH_PATH")" && pwd -P)

run_path_test "approot defaults to config" "approot; pwd" "$CONFIG_EXPECTED"
run_path_test "approot config target" "approot config; pwd" "$CONFIG_EXPECTED"
run_path_test "approot cache target" "approot cache; pwd" "$CACHE_EXPECTED"
run_path_test "approot completions target" "approot completions; pwd" "$COMPLETIONS_EXPECTED"
run_path_test "approot env target" "approot env; pwd" "$HOME_EXPECTED"
run_path_test "approot cjshenv target" "approot cjshenv; pwd" "$HOME_EXPECTED"
run_path_test "approot profile target" "approot profile; pwd" "$HOME_EXPECTED"
run_path_test "approot cjprofile target" "approot cjprofile; pwd" "$HOME_EXPECTED"
run_path_test "approot rc target" "approot rc; pwd" "$HOME_EXPECTED"
run_path_test "approot cjshrc target" "approot cjshrc; pwd" "$HOME_EXPECTED"
run_path_test "approot logout target" "approot logout; pwd" "$HOME_EXPECTED"
run_path_test "approot cjlogout target" "approot cjlogout; pwd" "$HOME_EXPECTED"
run_path_test "approot home target" "approot home; pwd" "$HOME_EXPECTED"
run_path_test "approot cjsh target" "approot cjsh; pwd" "$CJSH_EXPECTED"
run_path_test "builtin approot dispatch" "builtin approot cache; pwd" "$CACHE_EXPECTED"

SYMLINK_DIR=$(mktemp -d 2>/dev/null)
if [ -n "$SYMLINK_DIR" ] && [ -d "$SYMLINK_DIR" ]; then
    SYMLINK_CJSH="$SYMLINK_DIR/cjsh"
    if ln -s "$CJSH_PATH" "$SYMLINK_CJSH" >/dev/null 2>&1; then
        OUT=$("$SYMLINK_CJSH" -c "approot cjsh; pwd")
        STATUS=$?
        if [ "$STATUS" -eq 0 ] && [ "$OUT" = "$CJSH_EXPECTED" ]; then
            pass_test "approot cjsh resolves symlinked launcher"
        else
            fail_test "approot cjsh symlink resolution (status=$STATUS, out='$OUT')"
        fi
    else
        fail_test "approot cjsh symlink resolution (failed to create symlink)"
    fi
    rm -rf "$SYMLINK_DIR"
else
    fail_test "approot cjsh symlink resolution (mktemp unavailable)"
fi

OUT=$("$CJSH_PATH" -c "approot unknown-target" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 2 ] && printf "%s" "$OUT" | grep -q "unknown target"; then
    pass_test "approot rejects invalid target"
else
    fail_test "approot invalid target handling (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "approot config cache" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 2 ] && printf "%s" "$OUT" | grep -q "too many arguments"; then
    pass_test "approot rejects too many arguments"
else
    fail_test "approot too-many-arguments handling (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "approot --help" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 0 ] && printf "%s" "$OUT" | grep -q "Usage: approot"; then
    pass_test "approot --help prints usage"
else
    fail_test "approot --help output (status=$STATUS, out='$OUT')"
fi

echo ""
echo "Approot Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ "$TESTS_FAILED" -eq 0 ]; then
    echo "PASS"
    exit 0
fi

echo "FAIL"
exit 1
