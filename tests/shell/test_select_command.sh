#!/usr/bin/env sh
# test_select_command.sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: select command coverage..."

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

contains() {
    printf '%s\n' "$1" | grep -q "$2"
}

not_contains() {
    ! printf '%s\n' "$1" | grep -q "$2"
}

echo "Test inline select selects by number"
output=$(printf '2\n' | "$CJSH_PATH" -c 'select choice in alpha beta; do printf "choice=%s reply=%s\n" "$choice" "$REPLY"; break; done' 2>&1)
if contains "$output" '^1) alpha$' &&
   contains "$output" '^2) beta$' &&
   contains "$output" '#? choice=beta reply=2'; then
    pass_test "inline select selects by number"
else
    fail_test "inline select selects by number, got: '$output'"
fi

echo "Test multiline select body"
output=$(printf '1\n' | "$CJSH_PATH" -c 'select choice in alpha beta
do
    printf "choice=%s reply=%s\n" "$choice" "$REPLY"
    break
done' 2>&1)
if contains "$output" '#? choice=alpha reply=1'; then
    pass_test "multiline select body"
else
    fail_test "multiline select body, got: '$output'"
fi

echo "Test select with quoted item"
output=$(printf '1\n' | "$CJSH_PATH" -c 'select choice in "alpha beta" gamma; do printf "choice=%s reply=%s\n" "$choice" "$REPLY"; break; done' 2>&1)
if contains "$output" '^1) alpha beta$' &&
   contains "$output" '#? choice=alpha beta reply=1'; then
    pass_test "select preserves quoted items"
else
    fail_test "select preserves quoted items, got: '$output'"
fi

echo "Test blank input redisplays menu without running body"
output=$(printf '\n2\n' | "$CJSH_PATH" -c 'count=0; select choice in alpha beta; do count=$((count + 1)); printf "body=%s choice=%s reply=%s\n" "$count" "$choice" "$REPLY"; break; done; printf "count=%s\n" "$count"' 2>&1)
menu_count=$(printf '%s\n' "$output" | grep -c '^1) alpha$')
redisplay_count=$(printf '%s\n' "$output" | grep -c '#? 1) alpha')
if [ "$menu_count" -eq 1 ] &&
   [ "$redisplay_count" -eq 1 ] &&
   contains "$output" '#? body=1 choice=beta reply=2' &&
   contains "$output" '^count=1$' &&
   not_contains "$output" 'body=.*reply=$'; then
    pass_test "blank input redisplays menu without running body"
else
    fail_test "blank input redisplays menu without running body, got: '$output'"
fi

echo "Test non-numeric invalid choice"
output=$(printf 'bogus\n' | "$CJSH_PATH" -c 'select choice in alpha beta; do printf "choice=<%s> reply=%s\n" "$choice" "$REPLY"; break; done' 2>&1)
if contains "$output" '#? choice=<> reply=bogus'; then
    pass_test "non-numeric invalid choice leaves selected value empty"
else
    fail_test "non-numeric invalid choice, got: '$output'"
fi

echo "Test out-of-range invalid choice"
output=$(printf '9\n' | "$CJSH_PATH" -c 'select choice in alpha beta; do printf "choice=<%s> reply=%s\n" "$choice" "$REPLY"; break; done' 2>&1)
if contains "$output" '#? choice=<> reply=9'; then
    pass_test "out-of-range invalid choice leaves selected value empty"
else
    fail_test "out-of-range invalid choice, got: '$output'"
fi

echo "Test continue keeps select loop running"
output=$(printf '1\n2\n' | "$CJSH_PATH" -c 'select choice in alpha beta; do printf "seen=%s choice=%s\n" "$REPLY" "$choice"; continue; done; printf "after=%s\n" "$?"' 2>&1)
if contains "$output" '#? seen=1 choice=alpha' &&
   contains "$output" '#? seen=2 choice=beta' &&
   contains "$output" '^after=1$'; then
    pass_test "continue keeps select loop running"
else
    fail_test "continue keeps select loop running, got: '$output'"
fi

echo "Test break status from select"
output=$(printf '1\n' | "$CJSH_PATH" -c 'select choice in alpha beta; do break; done; printf "after=%s\n" "$?"' 2>&1)
if contains "$output" '#? after=0'; then
    pass_test "break exits select with status 0"
else
    fail_test "break exits select with status 0, got: '$output'"
fi

echo "Test EOF status from select"
output=$("$CJSH_PATH" -c 'select choice in alpha beta; do printf "body\n"; done; printf "after=%s\n" "$?"' < /dev/null 2>&1)
if contains "$output" '^after=1$' &&
   not_contains "$output" '^body$'; then
    pass_test "EOF exits select with status 1 without running body"
else
    fail_test "EOF exits select with status 1 without running body, got: '$output'"
fi

echo "Test select without in uses positional parameters"
output=$(printf '2\n' | "$CJSH_PATH" -c 'select choice; do printf "choice=%s reply=%s\n" "$choice" "$REPLY"; break; done' argv0 one two 2>&1)
if contains "$output" '^1) one$' &&
   contains "$output" '^2) two$' &&
   contains "$output" '#? choice=two reply=2'; then
    pass_test "select without in uses positional parameters"
else
    fail_test "select without in uses positional parameters, got: '$output'"
fi

echo "Test select without in and without positional parameters"
output=$("$CJSH_PATH" -c 'select choice; do printf "body\n"; done; printf "after=%s\n" "$?"' 2>&1)
if contains "$output" '^after=0$' &&
   not_contains "$output" '^body$' &&
   not_contains "$output" '#?'; then
    pass_test "select without positional parameters does not run body"
else
    fail_test "select without positional parameters, got: '$output'"
fi

echo "Test custom PS3 is literal"
output=$(printf '1\n' | "$CJSH_PATH" -c 'PS3="\u> "; select choice in alpha; do printf "choice=%s\n" "$choice"; break; done' 2>&1)
if contains "$output" '\\u> choice=alpha' &&
   not_contains "$output" '^[^\\].*> choice=alpha'; then
    pass_test "PS3 prompt is printed literally"
else
    fail_test "PS3 prompt should be literal, got: '$output'"
fi

echo "Test nested select"
output=$(printf '1\n2\n' | "$CJSH_PATH" -c 'select outer in alpha beta
do
    select inner in one two
    do
        printf "outer=%s inner=%s reply=%s\n" "$outer" "$inner" "$REPLY"
        break
    done
    break
done' 2>&1)
if contains "$output" '^1) alpha$' &&
   contains "$output" '#? 1) one' &&
   contains "$output" '#? outer=alpha inner=two'; then
    pass_test "nested select executes inner loop"
else
    fail_test "nested select executes inner loop, got: '$output'"
fi

echo "Test empty in list is a syntax error"
output=$("$CJSH_PATH" -c 'select choice in; do printf "body\n"; done' 2>&1)
exit_code=$?
if [ "$exit_code" -ne 0 ] &&
   not_contains "$output" '^body$'; then
    pass_test "empty in list is rejected"
else
    fail_test "empty in list should be rejected, exit=$exit_code got: '$output'"
fi

if [ $TESTS_FAILED -eq 0 ]; then
    echo "All select command tests passed!"
    exit 0
else
    echo "$TESTS_FAILED select command tests failed"
    exit 1
fi
