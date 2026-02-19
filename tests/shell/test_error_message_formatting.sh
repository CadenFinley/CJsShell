#!/usr/bin/env sh

TOTAL=0
PASSED=0
FAILED=0

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
DEFAULT_SHELL="$SCRIPT_DIR/../../build/cjsh"

if [ -n "${1-}" ]; then
    SHELL_TO_TEST="$1"
elif [ -n "${CJSH-}" ]; then
    SHELL_TO_TEST="$CJSH"
else
    SHELL_TO_TEST="$DEFAULT_SHELL"
fi

if [ "${SHELL_TO_TEST#/}" = "$SHELL_TO_TEST" ]; then
    SHELL_TO_TEST="$(pwd)/$SHELL_TO_TEST"
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

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
    exit 1
fi

TMPDIR_BASE=${TMPDIR:-/tmp}
WORK_DIR=$(mktemp -d "$TMPDIR_BASE/cjsh-error-format.XXXXXX")
trap 'chmod -R u+rw "$WORK_DIR" >/dev/null 2>&1; rm -rf "$WORK_DIR"' EXIT

NOPERM_FILE="$WORK_DIR/noperm_file"
printf "secret\n" > "$NOPERM_FILE"
chmod 000 "$NOPERM_FILE"

run_cmd() {
    cmd="$1"
    output=$("$SHELL_TO_TEST" -c "$cmd" 2>&1)
    status=$?
}

expect_nonempty_output_nonzero() {
    name="$1"
    cmd="$2"
    log_test "$name"
    run_cmd "$cmd"
    if [ -n "$output" ] && [ $status -ne 0 ]; then
        pass
    else
        fail "Expected output and non-zero status; status=$status output='$output'"
    fi
}

expect_output_contains() {
    name="$1"
    cmd="$2"
    needle="$3"
    log_test "$name"
    run_cmd "$cmd"
    if [ -n "$output" ] && printf '%s' "$output" | grep -q "$needle"; then
        pass
    else
        fail "Expected output containing '$needle'; status=$status output='$output'"
    fi
}

expect_output_not_contains() {
    name="$1"
    cmd="$2"
    needle="$3"
    log_test "$name"
    run_cmd "$cmd"
    if printf '%s' "$output" | grep -q "$needle"; then
        fail "Unexpected output containing '$needle'; status=$status output='$output'"
    else
        pass
    fi
}

expect_invalid_option_message() {
    name="$1"
    cmd="$2"
    log_test "$name"
    run_cmd "$cmd"
    if [ -n "$output" ] && [ $status -ne 0 ] && printf '%s' "$output" | grep -E -q "invalid option|invalid argument"; then
        pass
    else
        fail "Expected invalid option/argument message; status=$status output='$output'"
    fi
}

expect_no_escape_sequences() {
    name="$1"
    cmd="$2"
    log_test "$name"
    run_cmd "$cmd"
    if [ -n "$output" ] && printf '%s' "$output" | awk 'BEGIN{esc=sprintf("%c",27)} index($0, esc){found=1} END{exit found}'; then
        fail "Unexpected escape sequence in output; status=$status output='$output'"
    else
        pass
    fi
}

expect_nonempty_output_nonzero "syntax error reports message" "if then; fi"
expect_nonempty_output_nonzero "bad parameter expansion reports message" "unset FOO; echo \${FOO?missing}"
expect_nonempty_output_nonzero "bad arithmetic expansion reports message" "echo \$((1/0))"
expect_nonempty_output_nonzero "bad test bracket reports message" "[ 1 -eq 1"
expect_nonempty_output_nonzero "bad double-bracket reports message" "[[ 1 -eq ]]"
expect_nonempty_output_nonzero "source directory reports message" "source $WORK_DIR"
expect_nonempty_output_nonzero "shift too far reports message" "set -- a b; shift 5"
expect_nonempty_output_nonzero "return outside function reports message" "return 1"
expect_nonempty_output_nonzero "break outside loop reports message" "break"
expect_nonempty_output_nonzero "continue outside loop reports message" "continue"
expect_output_contains "exit invalid numeric reports message" "exit nope" "invalid"
expect_nonempty_output_nonzero "readonly invalid name reports message" "readonly 1abc=3"
expect_nonempty_output_nonzero "unset invalid name reports message" "unset 1abc"
expect_nonempty_output_nonzero "trap invalid signal reports message" "trap 'echo hi' 99999"
expect_nonempty_output_nonzero "test syntax error reports message" "[ 1 -eq ]"
expect_nonempty_output_nonzero "if bad syntax reports message" "if true; fi"
expect_nonempty_output_nonzero "getopts illegal option reports message" "getopts a opt -z"
expect_invalid_option_message "history invalid option reports message" "history -z"
expect_invalid_option_message "kill invalid option reports message" "kill -z 1"
expect_invalid_option_message "generate-completions invalid option reports message" "generate-completions --jobs nope"
expect_output_contains "source permission denied reports message" ". $NOPERM_FILE" "permission denied"
expect_output_contains "cjsh-widget no session reports message" "cjsh-widget get-buffer" "no active readline session"
expect_no_escape_sequences "cjsh-widget no session emits no escape codes" "cjsh-widget get-buffer"
expect_output_not_contains "history invalid option avoids interpreter error" "history -z" "unknown interpreter error"
expect_output_not_contains "kill invalid option avoids interpreter error" "kill -z 1" "unknown interpreter error"
expect_output_not_contains "generate-completions invalid option avoids interpreter error" "generate-completions --jobs nope" "unknown interpreter error"

echo ""
echo "================================================================"
echo "Error Message Formatting Test Results:"
echo "  Total tests: $TOTAL"
echo "  Passed:      ${GREEN}$PASSED${NC}"
echo "  Failed:      ${RED}$FAILED${NC}"
if [ $FAILED -eq 0 ]; then
    echo "  ${GREEN}All error message formatting tests passed!${NC}"
    exit 0
else
    echo "  ${RED}Some error message formatting tests failed.${NC}"
    exit 1
fi
