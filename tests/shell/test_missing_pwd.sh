#!/usr/bin/env sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: resilience when the working directory vanishes"

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

ensure_cjsh_available() {
    if [ ! -x "$CJSH_PATH" ]; then
        fail_test "cjsh binary not found at $CJSH_PATH"
        return 1
    fi
    return 0
}

run_in_deleted_directory() {
    tmp_dir=$(mktemp -d 2>/dev/null)
    if [ -z "$tmp_dir" ] || [ ! -d "$tmp_dir" ]; then
        return 1
    fi

    (
        cd "$tmp_dir" || exit 1
        mkdir ghost && cd ghost || exit 2
        deleted_pwd=$(pwd)
        rm -rf "$tmp_dir" || exit 3
        DELETED_DIR_PATH="$deleted_pwd" "$@"
    )

    return $?
}

scenario_echo_ready() {
    output=$("$CJSH_PATH" -c 'echo READY' 2>&1) || return 10
    printf "%s\n" "$output" | grep -q '^READY$' || return 11
}

scenario_cd_and_pwd() {
    output=$("$CJSH_PATH" -c 'cd / && pwd' 2>&1) || return 20
    [ "$output" = "/" ] || return 21
}

scenario_pwd_only() {
    output=$("$CJSH_PATH" -c 'pwd' 2>&1) || return 30
    if [ -n "$DELETED_DIR_PATH" ]; then
        [ "$output" = "$DELETED_DIR_PATH" ] || return 31
    fi
}

test_noninteractive_command_after_cwd_removal() {
    TEST_NAME="non-interactive command still runs after cwd deletion"

    run_in_deleted_directory scenario_echo_ready

    if [ $? -eq 0 ]; then
        pass_test "$TEST_NAME"
    else
        fail_test "$TEST_NAME"
    fi
}

test_change_directory_and_pwd_after_cwd_removal() {
    TEST_NAME="can cd away from deleted cwd and run pwd"

    run_in_deleted_directory scenario_cd_and_pwd

    if [ $? -eq 0 ]; then
        pass_test "$TEST_NAME"
    else
        fail_test "$TEST_NAME"
    fi
}

test_pwd_in_deleted_dir() {
    TEST_NAME="pwd reports previous location even when directory is gone"

    run_in_deleted_directory scenario_pwd_only

    if [ $? -eq 0 ]; then
        pass_test "$TEST_NAME"
    else
        fail_test "$TEST_NAME"
    fi
}

if ensure_cjsh_available; then
    test_noninteractive_command_after_cwd_removal
    test_change_directory_and_pwd_after_cwd_removal
    test_pwd_in_deleted_dir
fi

echo ""
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
exit $TESTS_FAILED
