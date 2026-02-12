# POSIX mode restriction tests

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

tmp_source_file=$(mktemp)
echo 'echo from source' > "$tmp_source_file"

echo "Testing POSIX mode restrictions for: $SHELL_TO_TEST"
echo "==================================================="

run_expect_fail() {
    local description=$1
    local command=$2
    local pattern=$3

    log_test "$description"
    output=$($SHELL_TO_TEST --posix -c "$command" 2>&1)
    status=$?

    if [ $status -ne 0 ] && printf "%s" "$output" | grep -q "$pattern"; then
        pass
    else
        clean_output=$(printf "%s" "$output" | tr '\n' ' ')
        fail "Expected failure with pattern '$pattern' (status=$status, output=$clean_output)"
    fi
}

run_expect_literal() {
    local description=$1
    local command=$2
    local expected=$3

    log_test "$description"
    output=$($SHELL_TO_TEST --posix -c "$command" 2>/dev/null)
    if [ "$output" = "$expected" ]; then
        pass
    else
        fail "Expected '$expected', got '$output'"
    fi
}

run_expect_fail "[[ forbidden" "[[ 1 == 1 ]]" "POSIX001"
run_expect_fail "function keyword disabled" "function foo { :; }; foo" "POSIX002"
run_expect_fail "array assignment disabled" "arr=(1 2)" "POSIX005"
run_expect_fail "+= assignment disabled" "x=1; x+=2" "POSIX006"
run_expect_fail "|& pipeline disabled" "echo hi |& cat" "POSIX007"
run_expect_fail "&> redirection disabled" "echo hi &> '$tmpdir/out.txt'" "POSIX008"
run_expect_fail "here-string disabled" "cat <<< hi" "POSIX004"
run_expect_fail "process substitution disabled" "cat <(echo hi)" "POSIX003"
run_expect_fail "source builtin disabled" "source '$tmp_source_file'" "disabled in POSIX mode"
run_expect_fail "local builtin disabled" "local foo=1" "disabled in POSIX mode"
run_expect_literal "dot builtin allowed" ". '$tmp_source_file'" "from source"

run_expect_literal "brace expansion stays literal" "echo {1..3}" "{1..3}"
run_expect_literal "tilde stays literal" "HOME=/tmp/cjsh_posix_home; echo ~" "~"

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir" "$tmp_source_file"' EXIT

log_test "globstar remains literal when disabled"
glob_output=$($SHELL_TO_TEST --posix -c "cd '$tmpdir' && echo **" 2>/dev/null)
if [ "$glob_output" = "**" ]; then
    pass
else
    fail "Expected literal '**', got '$glob_output'"
fi

echo "==================================================="
echo "POSIX Mode Restriction Tests Summary:"
echo "Total: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
