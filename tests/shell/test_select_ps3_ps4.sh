#!/usr/bin/env sh
# test_select_ps3_ps4.sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: select PS3 and xtrace PS4 behavior..."

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

output=$("$CJSH_PATH" -c 'set -x; echo hello' 2>&1)
if echo "$output" | grep -q '^+ echo hello$'; then
    pass_test "PS4 default is used by xtrace"
else
    fail_test "PS4 default should prefix xtrace with '+ ', got: '$output'"
fi

output=$("$CJSH_PATH" -c 'PS4="TRACE> "; set -x; echo hello' 2>&1)
if echo "$output" | grep -q '^TRACE> echo hello$'; then
    pass_test "custom PS4 is used by xtrace"
else
    fail_test "custom PS4 should prefix xtrace, got: '$output'"
fi

output=$("$CJSH_PATH" -c 'PS4="\u> "; set -x; echo hello' 2>&1)
if echo "$output" | grep -q '> echo hello$' &&
   ! echo "$output" | grep -q '\\u> echo hello'; then
    pass_test "PS4 expands prompt escapes"
else
    fail_test "PS4 should expand prompt escapes, got: '$output'"
fi

output=$("$CJSH_PATH" -c 'TRACE_PREFIX=TRACE; PS4="${TRACE_PREFIX}> "; set -x; echo hello' 2>&1)
if echo "$output" | grep -q '^TRACE> echo hello$'; then
    pass_test "PS4 expands shell variables at trace time"
else
    fail_test "PS4 should expand shell variables at trace time, got: '$output'"
fi

output=$(printf '2\n' | "$CJSH_PATH" -c 'select choice in alpha beta; do printf "choice=%s reply=%s\n" "$choice" "$REPLY"; break; done' 2>&1)
if echo "$output" | grep -q '^1) alpha$' &&
   echo "$output" | grep -q '^2) beta$' &&
   echo "$output" | grep -q '#? choice=beta reply=2'; then
    pass_test "select uses default PS3 prompt and selected value"
else
    fail_test "select should use default PS3 and selected value, got: '$output'"
fi

output=$(printf '3\n' | "$CJSH_PATH" -c 'PS3="pick> "; select choice in alpha beta; do printf "choice=<%s> reply=%s\n" "$choice" "$REPLY"; break; done' 2>&1)
if echo "$output" | grep -q 'pick> choice=<> reply=3'; then
    pass_test "select uses custom PS3 and empty value for invalid selection"
else
    fail_test "select should use custom PS3 and empty invalid selection, got: '$output'"
fi

output=$(printf '2\n' | "$CJSH_PATH" -c 'select choice; do printf "choice=%s reply=%s\n" "$choice" "$REPLY"; break; done' argv0 one two 2>&1)
if echo "$output" | grep -q '^1) one$' &&
   echo "$output" | grep -q '^2) two$' &&
   echo "$output" | grep -q '#? choice=two reply=2'; then
    pass_test "select without in uses positional parameters"
else
    fail_test "select without in should use positional parameters, got: '$output'"
fi

if [ $TESTS_FAILED -eq 0 ]; then
    echo "All select/PS3/PS4 tests passed!"
    exit 0
else
    echo "$TESTS_FAILED select/PS3/PS4 tests failed"
    exit 1
fi
