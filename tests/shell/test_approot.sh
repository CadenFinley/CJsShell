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

export CJSH_ENV=""
export CJSH_HISTORY_FILE=""

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

resolve_startup_path_with_fallback() {
    primary_path=$1
    alternate_path=$2
    require_regular_file=$3

    if [ "$require_regular_file" = "yes" ]; then
        if [ -f "$primary_path" ]; then
            printf '%s\n' "$primary_path"
            return
        fi

        if [ -f "$alternate_path" ]; then
            printf '%s\n' "$alternate_path"
            return
        fi

        printf '%s\n' "$primary_path"
        return
    fi

    if [ -e "$primary_path" ]; then
        printf '%s\n' "$primary_path"
        return
    fi

    if [ -e "$alternate_path" ]; then
        printf '%s\n' "$alternate_path"
        return
    fi

    printf '%s\n' "$primary_path"
}

CONFIG_DIR="$HOME/.config/cjsh"
CACHE_DIR="$HOME/.cache/cjsh"
COMPLETIONS_DIR="$CACHE_DIR/generated_completions"

mkdir -p "$CONFIG_DIR" "$COMPLETIONS_DIR"

CONFIG_EXPECTED=$(cd "$CONFIG_DIR" && pwd -P)
CACHE_EXPECTED=$(cd "$CACHE_DIR" && pwd -P)
FIRST_BOOT_EXPECTED=$(cd "$CACHE_DIR" && pwd -P)
COMPLETIONS_EXPECTED=$(cd "$COMPLETIONS_DIR" && pwd -P)
HOME_EXPECTED=$(cd "$HOME" && pwd -P)
CJSH_EXPECTED=$(cd "$(dirname "$CJSH_PATH")" && pwd -P)
START_EXPECTED=$(pwd -P)
HISTORY_FILE_EXPECTED="$CACHE_EXPECTED/history.txt"
FIRST_BOOT_FILE_EXPECTED="$CACHE_EXPECTED/.first_boot"
ENV_FILE_EXPECTED=$(resolve_startup_path_with_fallback "$HOME_EXPECTED/.cjshenv" "$CONFIG_EXPECTED/.cjshenv" "yes")
PROFILE_FILE_EXPECTED=$(resolve_startup_path_with_fallback "$HOME_EXPECTED/.cjprofile" "$CONFIG_EXPECTED/.cjprofile" "no")
RC_FILE_EXPECTED=$(resolve_startup_path_with_fallback "$HOME_EXPECTED/.cjshrc" "$CONFIG_EXPECTED/.cjshrc" "no")
LOGOUT_FILE_EXPECTED=$(resolve_startup_path_with_fallback "$HOME_EXPECTED/.cjlogout" "$CONFIG_EXPECTED/.cjlogout" "no")
ENV_EXPECTED=$(dirname "$ENV_FILE_EXPECTED")
PROFILE_EXPECTED=$(dirname "$PROFILE_FILE_EXPECTED")
RC_EXPECTED=$(dirname "$RC_FILE_EXPECTED")
LOGOUT_EXPECTED=$(dirname "$LOGOUT_FILE_EXPECTED")

run_path_test "approot defaults to config" "approot; pwd" "$CONFIG_EXPECTED"
run_path_test "approot config target" "approot config; pwd" "$CONFIG_EXPECTED"
run_path_test "approot cache target" "approot cache; pwd" "$CACHE_EXPECTED"
run_path_test "approot history target" "approot history; pwd" "$CACHE_EXPECTED"
run_path_test "approot firstboot target" "approot firstboot; pwd" "$FIRST_BOOT_EXPECTED"
run_path_test "approot first_boot target" "approot first_boot; pwd" "$FIRST_BOOT_EXPECTED"
run_path_test "approot completions target" "approot completions; pwd" "$COMPLETIONS_EXPECTED"
run_path_test "approot env target" "approot env; pwd" "$ENV_EXPECTED"
run_path_test "approot cjshenv target" "approot cjshenv; pwd" "$ENV_EXPECTED"
run_path_test "approot profile target" "approot profile; pwd" "$PROFILE_EXPECTED"
run_path_test "approot cjprofile target" "approot cjprofile; pwd" "$PROFILE_EXPECTED"
run_path_test "approot rc target" "approot rc; pwd" "$RC_EXPECTED"
run_path_test "approot cjshrc target" "approot cjshrc; pwd" "$RC_EXPECTED"
run_path_test "approot logout target" "approot logout; pwd" "$LOGOUT_EXPECTED"
run_path_test "approot cjlogout target" "approot cjlogout; pwd" "$LOGOUT_EXPECTED"
run_path_test "approot home target" "approot home; pwd" "$HOME_EXPECTED"
run_path_test "approot cjsh target" "approot cjsh; pwd" "$CJSH_EXPECTED"
run_path_test "builtin approot dispatch" "builtin approot cache; pwd" "$CACHE_EXPECTED"
run_path_test "approot --print default target" "approot --print" "$CONFIG_EXPECTED"
run_path_test "approot --print cache target" "approot --print cache" "$CACHE_EXPECTED"
run_path_test "approot --print history target" "approot --print history" "$CACHE_EXPECTED"
run_path_test "approot --print firstboot target" "approot --print firstboot" "$FIRST_BOOT_EXPECTED"
run_path_test "approot -p cache target" "approot -p cache" "$CACHE_EXPECTED"
run_path_test "approot -p history target" "approot -p history" "$CACHE_EXPECTED"
run_path_test "approot -p firstboot target" "approot -p firstboot" "$FIRST_BOOT_EXPECTED"
run_path_test "approot --file history path" "approot --file history" "$HISTORY_FILE_EXPECTED"
run_path_test "approot -f history path" "approot -f history" "$HISTORY_FILE_EXPECTED"
run_path_test "approot --file firstboot path" "approot --file firstboot" "$FIRST_BOOT_FILE_EXPECTED"
run_path_test "approot --file env path" "approot --file env" "$ENV_FILE_EXPECTED"
run_path_test "approot --file profile path" "approot --file profile" "$PROFILE_FILE_EXPECTED"
run_path_test "approot --file rc path" "approot --file rc" "$RC_FILE_EXPECTED"
run_path_test "approot --file logout path" "approot --file logout" "$LOGOUT_FILE_EXPECTED"
run_path_test "approot --file cache stays directory" "approot --file cache" "$CACHE_EXPECTED"
run_path_test "approot --print keeps cwd unchanged" "approot --print cache >/dev/null; pwd" "$START_EXPECTED"
run_path_test "approot --print works in command substitution" "cd \"\$(approot --print cache)\"; pwd" "$CACHE_EXPECTED"
run_path_test "approot --file keeps cwd unchanged" "approot --file history >/dev/null; pwd" "$START_EXPECTED"

ALT_PROFILE_HOME=$(mktemp -d 2>/dev/null)
if [ -n "$ALT_PROFILE_HOME" ] && [ -d "$ALT_PROFILE_HOME" ]; then
    ALT_CONFIG_DIR="$ALT_PROFILE_HOME/.config/cjsh"
    ALT_PROFILE_FILE="$ALT_CONFIG_DIR/.cjprofile"
    ALT_PROFILE_DIR_EXPECTED="$ALT_CONFIG_DIR"
    ALT_PROFILE_FILE_EXPECTED="$ALT_PROFILE_FILE"

    mkdir -p "$ALT_CONFIG_DIR"
    : > "$ALT_PROFILE_FILE"

    OUT=$(HOME="$ALT_PROFILE_HOME" "$CJSH_PATH" -c "approot --print profile")
    STATUS=$?
    if [ "$STATUS" -eq 0 ] && [ "$OUT" = "$ALT_PROFILE_DIR_EXPECTED" ]; then
        pass_test "approot profile falls back to config profile path"
    else
        fail_test "approot profile fallback path (status=$STATUS, out='$OUT')"
    fi

    OUT=$(HOME="$ALT_PROFILE_HOME" "$CJSH_PATH" -c "approot --file profile")
    STATUS=$?
    if [ "$STATUS" -eq 0 ] && [ "$OUT" = "$ALT_PROFILE_FILE_EXPECTED" ]; then
        pass_test "approot --file profile falls back to config profile file"
    else
        fail_test "approot --file profile fallback path (status=$STATUS, out='$OUT')"
    fi

    rm -rf "$ALT_PROFILE_HOME"
else
    fail_test "approot profile fallback tests (mktemp unavailable)"
fi

ENV_OVERRIDE_DIR=$(mktemp -d 2>/dev/null)
if [ -n "$ENV_OVERRIDE_DIR" ] && [ -d "$ENV_OVERRIDE_DIR" ]; then
    ENV_OVERRIDE_FILE="$ENV_OVERRIDE_DIR/cjsh-env.override"
    ENV_OVERRIDE_EXPECTED=$(cd "$ENV_OVERRIDE_DIR" && pwd)
    ENV_OVERRIDE_FILE_EXPECTED="$ENV_OVERRIDE_FILE"
    TILDE_ENV_OVERRIDE_FILE="$HOME/cjsh_tilde_env_override.$$"
    TILDE_ENV_OVERRIDE_BASENAME=$(basename "$TILDE_ENV_OVERRIDE_FILE")
    TILDE_ENV_OVERRIDE_EXPECTED="$HOME_EXPECTED"
    TILDE_ENV_OVERRIDE_FILE_EXPECTED="$TILDE_ENV_OVERRIDE_FILE"

    : > "$TILDE_ENV_OVERRIDE_FILE"

    run_path_test "approot env honors missing CJSH_ENV file" \
        "export CJSH_ENV='$ENV_OVERRIDE_FILE'; approot env; pwd" "$ENV_OVERRIDE_EXPECTED"
    run_path_test "approot cjshenv honors missing CJSH_ENV file" \
        "export CJSH_ENV='$ENV_OVERRIDE_FILE'; approot cjshenv; pwd" "$ENV_OVERRIDE_EXPECTED"
    run_path_test "approot --file env honors missing CJSH_ENV file" \
        "export CJSH_ENV='$ENV_OVERRIDE_FILE'; approot --file env" "$ENV_OVERRIDE_FILE_EXPECTED"
    run_path_test "approot --file cjshenv honors missing CJSH_ENV file" \
        "export CJSH_ENV='$ENV_OVERRIDE_FILE'; approot --file cjshenv" "$ENV_OVERRIDE_FILE_EXPECTED"
    run_path_test "approot env expands leading tilde in CJSH_ENV" \
        "export CJSH_ENV='~/$TILDE_ENV_OVERRIDE_BASENAME'; approot env; pwd" "$TILDE_ENV_OVERRIDE_EXPECTED"
    run_path_test "approot --file env expands leading tilde in CJSH_ENV" \
        "export CJSH_ENV='~/$TILDE_ENV_OVERRIDE_BASENAME'; approot --file env" "$TILDE_ENV_OVERRIDE_FILE_EXPECTED"

    rm -f "$TILDE_ENV_OVERRIDE_FILE"
    rm -rf "$ENV_OVERRIDE_DIR"
else
    fail_test "approot env override handling (mktemp unavailable)"
fi

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

OUT=$("$CJSH_PATH" -c "approot --not-an-option" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 2 ] && printf "%s" "$OUT" | grep -q "invalid option"; then
    pass_test "approot rejects invalid options"
else
    fail_test "approot invalid-option handling (status=$STATUS, out='$OUT')"
fi

REDIRECT_DIR=$(mktemp -d 2>/dev/null)
if [ -n "$REDIRECT_DIR" ] && [ -d "$REDIRECT_DIR" ]; then
    STDOUT_FILE="$REDIRECT_DIR/stdout.txt"
    STDERR_FILE="$REDIRECT_DIR/stderr.txt"

    STATUS=0
    "$CJSH_PATH" -c "approot --print cache > '$STDOUT_FILE' 2> '$STDERR_FILE'" || STATUS=$?
    STDOUT_CONTENT=$(tr -d '\n' < "$STDOUT_FILE")
    STDERR_EMPTY="no"
    if [ ! -s "$STDERR_FILE" ]; then
        STDERR_EMPTY="yes"
    fi

    if [ "$STATUS" -eq 0 ] && [ "$STDOUT_CONTENT" = "$CACHE_EXPECTED" ] &&
        [ "$STDERR_EMPTY" = "yes" ]; then
        pass_test "approot --print supports stdout redirection"
    else
        STDERR_CONTENT=$(tr -d '\n' < "$STDERR_FILE")
        fail_test "approot --print stdout redirection (status=$STATUS, out='$STDOUT_CONTENT', err='$STDERR_CONTENT')"
    fi

    STATUS=0
    "$CJSH_PATH" -c "approot --print unknown-target > '$STDOUT_FILE' 2> '$STDERR_FILE'" || STATUS=$?
    STDOUT_EMPTY="no"
    if [ ! -s "$STDOUT_FILE" ]; then
        STDOUT_EMPTY="yes"
    fi

    if [ "$STATUS" -eq 2 ] && [ "$STDOUT_EMPTY" = "yes" ] &&
        grep -q "unknown target" "$STDERR_FILE"; then
        pass_test "approot --print supports stderr redirection"
    else
        STDOUT_CONTENT=$(tr -d '\n' < "$STDOUT_FILE")
        STDERR_CONTENT=$(tr -d '\n' < "$STDERR_FILE")
        fail_test "approot --print stderr redirection (status=$STATUS, out='$STDOUT_CONTENT', err='$STDERR_CONTENT')"
    fi

    rm -rf "$REDIRECT_DIR"
else
    fail_test "approot redirection tests (mktemp unavailable)"
fi

OUT=$("$CJSH_PATH" -c "approot --help" 2>&1)
STATUS=$?
if [ "$STATUS" -eq 0 ] && printf "%s" "$OUT" | grep -q "Usage: approot" &&
    printf "%s" "$OUT" | grep -q -- "--print" &&
    printf "%s" "$OUT" | grep -q -- "--file"; then
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
