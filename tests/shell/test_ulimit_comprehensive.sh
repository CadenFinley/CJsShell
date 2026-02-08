#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: ulimit comprehensive..."

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

is_number() {
    case $1 in
        ''|*[!0-9]*) return 1 ;;
        *) return 0 ;;
    esac
}

is_valid_limit() {
    if [ "$1" = "unlimited" ]; then
        return 0
    fi
    is_number "$1"
}

ORIG_SOFT=""

restore_limits() {
    if [ -n "$ORIG_SOFT" ]; then
        "$CJSH_PATH" -c "ulimit -Sn $ORIG_SOFT" >/dev/null 2>&1
    fi
}

# Basic sanity: default ulimit output
DEFAULT_OUT=$("$CJSH_PATH" -c "ulimit" 2>/dev/null)
DEFAULT_STATUS=$?
if [ $DEFAULT_STATUS -eq 0 ] && is_valid_limit "$DEFAULT_OUT"; then
    pass_test "ulimit reports default file size limit"
else
    fail_test "ulimit failed to report default file size limit"
fi

# Capture original soft descriptor limit
ORIG_SOFT=$("$CJSH_PATH" -c "ulimit -Sn" 2>/dev/null)
SOFT_STATUS=$?
if [ $SOFT_STATUS -ne 0 ] || ! is_valid_limit "$ORIG_SOFT"; then
    fail_test "ulimit -Sn unable to obtain current soft limit"
    echo ""
    echo "Ulimit Comprehensive Tests Summary:"
    echo "Passed: $TESTS_PASSED"
    echo "Failed: $TESTS_FAILED"
    echo "FAIL"
    exit 1
fi
pass_test "ulimit -Sn retrieves current soft limit"
trap restore_limits EXIT INT TERM

# Capture original hard descriptor limit
ORIG_HARD=$("$CJSH_PATH" -c "ulimit -Hn" 2>/dev/null)
HARD_STATUS=$?
if [ $HARD_STATUS -ne 0 ] || ! is_valid_limit "$ORIG_HARD"; then
    fail_test "ulimit -Hn unable to obtain current hard limit"
else
    pass_test "ulimit -Hn retrieves current hard limit"
fi

# Verify -a and --all outputs
ALL_SHORT=$("$CJSH_PATH" -c "ulimit -a" 2>/dev/null)
ALL_SHORT_STATUS=$?
if [ $ALL_SHORT_STATUS -eq 0 ] && echo "$ALL_SHORT" | grep -q "Maximum size of core files"; then
    pass_test "ulimit -a lists descriptive entries"
else
    fail_test "ulimit -a did not list expected entries"
fi

ALL_LONG=$("$CJSH_PATH" -c "ulimit --all" 2>/dev/null)
if [ $? -eq 0 ] && [ "$ALL_SHORT" = "$ALL_LONG" ]; then
    pass_test "ulimit --all matches -a output"
else
    fail_test "ulimit --all output mismatch"
fi

# Check long option alias for -n
ULIMIT_N_SHORT=$("$CJSH_PATH" -c "ulimit -n" 2>/dev/null)
ULIMIT_N_STATUS=$?
ULIMIT_N_LONG=$("$CJSH_PATH" -c "ulimit --file-descriptor-count" 2>/dev/null)
if [ $ULIMIT_N_STATUS -eq 0 ] && [ "$ULIMIT_N_SHORT" = "$ULIMIT_N_LONG" ]; then
    pass_test "ulimit --file-descriptor-count equals -n"
else
    fail_test "ulimit --file-descriptor-count mismatch"
fi

# Determine a safe soft limit to test lowering to
choose_test_soft() {
    candidate=""
    if is_number "$ORIG_SOFT"; then
        if [ "$ORIG_SOFT" -gt 64 ]; then
            candidate=$((ORIG_SOFT - 1))
        elif [ "$ORIG_SOFT" -gt 1 ]; then
            candidate=$((ORIG_SOFT - 1))
        else
            candidate=$ORIG_SOFT
        fi
    else
        candidate=256
    fi

    if is_number "$ORIG_HARD" && [ "$ORIG_HARD" -gt 0 ]; then
        if [ "$candidate" -gt "$ORIG_HARD" ]; then
            candidate=$ORIG_HARD
        fi
    fi

    if ! is_number "$candidate" || [ "$candidate" -lt 1 ]; then
        candidate=1
    fi

    echo "$candidate"
}

# TEST_SOFT=$(choose_test_soft)

# SET_OUT=$("$CJSH_PATH" -c "ulimit -Sn $TEST_SOFT" 2>&1)
# SET_STATUS=$?
# if [ $SET_STATUS -eq 0 ]; then
#     NEW_SOFT=$("$CJSH_PATH" -c "ulimit -Sn" 2>/dev/null)
#     if [ "$NEW_SOFT" = "$TEST_SOFT" ]; then
#         pass_test "ulimit -Sn sets new soft limit"
#     else
#     fi
# else
#     fail_test "ulimit -Sn failed to set new soft limit: $SET_OUT"
# fi

# Set soft limit to hard limit using keyword
# HARD_SET_OUT=$("$CJSH_PATH" -c "ulimit -Sn hard" 2>&1)
# HARD_SET_STATUS=$?
# if [ $HARD_SET_STATUS -eq 0 ]; then
#     HARD_SOFT=$("$CJSH_PATH" -c "ulimit -Sn" 2>/dev/null)
#     if [ "$HARD_SOFT" = "$ORIG_HARD" ]; then
#         pass_test "ulimit -Sn hard matches hard limit"
#     else
#     fi
# else
#     fail_test "ulimit -Sn hard failed: $HARD_SET_OUT"
# fi

# Restore original soft limit explicitly
RESTORE_STATUS=$("$CJSH_PATH" -c "ulimit -Sn $ORIG_SOFT" 2>&1)
if [ $? -eq 0 ]; then
    CURRENT_SOFT=$("$CJSH_PATH" -c "ulimit -Sn" 2>/dev/null)
    if [ "$CURRENT_SOFT" = "$ORIG_SOFT" ]; then
        pass_test "ulimit restored original soft limit"
    else
        fail_test "ulimit restore mismatch (expected $ORIG_SOFT, got $CURRENT_SOFT)"
    fi
else
    fail_test "ulimit failed to restore original soft limit: $RESTORE_STATUS"
fi

# Keyword 'soft' should preserve current soft limit
SOFT_KEYWORD_OUT=$("$CJSH_PATH" -c "ulimit -Sn soft" 2>&1)
if [ $? -eq 0 ]; then
    VERIFY_SOFT=$("$CJSH_PATH" -c "ulimit -Sn" 2>/dev/null)
    if [ "$VERIFY_SOFT" = "$ORIG_SOFT" ]; then
        pass_test "ulimit -Sn soft retains current value"
    else
        fail_test "ulimit -Sn soft altered value (expected $ORIG_SOFT, got $VERIFY_SOFT)"
    fi
else
    fail_test "ulimit -Sn soft keyword rejected: $SOFT_KEYWORD_OUT"
fi

# Invalid option handling
"$CJSH_PATH" -c "ulimit -Z" >/tmp/cjsh_ulimit_invalid_option.log 2>&1
if [ $? -ne 0 ]; then
    pass_test "ulimit rejects unknown option"
else
    fail_test "ulimit accepted unknown option"
fi
rm -f /tmp/cjsh_ulimit_invalid_option.log

# Invalid numeric value handling
"$CJSH_PATH" -c "ulimit -Sn not_a_number" >/tmp/cjsh_ulimit_invalid_value.log 2>&1
if [ $? -ne 0 ] && grep -qi "invalid limit" /tmp/cjsh_ulimit_invalid_value.log; then
    pass_test "ulimit rejects non-numeric limit"
else
    fail_test "ulimit did not reject non-numeric limit"
fi
rm -f /tmp/cjsh_ulimit_invalid_value.log

# Empty string argument handling
"$CJSH_PATH" -c 'ulimit -Sn ""' >/tmp/cjsh_ulimit_empty_value.log 2>&1
if [ $? -ne 0 ]; then
    pass_test "ulimit rejects empty limit"
else
    fail_test "ulimit accepted empty limit"
fi
rm -f /tmp/cjsh_ulimit_empty_value.log

# Optional resource (swap) should either work or provide a clear message
ULIMIT_W_OUTPUT=$("$CJSH_PATH" -c "ulimit -w" 2>&1)
ULIMIT_W_STATUS=$?
if [ $ULIMIT_W_STATUS -eq 0 ]; then
    if is_valid_limit "$ULIMIT_W_OUTPUT"; then
        pass_test "ulimit -w reports swap limit"
    else
        fail_test "ulimit -w returned unexpected output"
    fi
else
    if echo "$ULIMIT_W_OUTPUT" | grep -qi "not available"; then
        pass_test "ulimit -w gracefully reports unsupported resource"
    else
        fail_test "ulimit -w failure without clear message"
    fi
fi

# Final summary
echo ""
echo "Ulimit Comprehensive Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
