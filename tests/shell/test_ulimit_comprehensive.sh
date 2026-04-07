#!/usr/bin/env sh

# test_ulimit_comprehensive.sh
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

# Help output should include the full selector table
HELP_OUT=$("$CJSH_PATH" -c "ulimit --help" 2>/dev/null)
if echo "$HELP_OUT" | grep -q -- "-p, --pipe-size"; then
    pass_test "ulimit --help lists pipe-size selector"
else
    fail_test "ulimit --help missing pipe-size selector"
fi

# Keyword 'soft' should preserve current soft limit (single shell process)
SOFT_KEYWORD_OUT=$("$CJSH_PATH" -c "orig=\$(ulimit -Sn); ulimit -Sn soft && after=\$(ulimit -Sn); printf '%s %s\n' \"\$orig\" \"\$after\"" 2>&1)
if [ $? -eq 0 ]; then
    set -- $SOFT_KEYWORD_OUT
    if [ $# -eq 2 ] && [ "$1" = "$2" ]; then
        pass_test "ulimit -Sn soft retains current value"
    else
        fail_test "ulimit -Sn soft altered value (got '$SOFT_KEYWORD_OUT')"
    fi
else
    fail_test "ulimit -Sn soft keyword rejected: $SOFT_KEYWORD_OUT"
fi

# Keyword 'hard' should set soft to current hard limit (single shell process)
HARD_KEYWORD_OUT=$("$CJSH_PATH" -c "hard=\$(ulimit -Hn); ulimit -Sn hard && soft=\$(ulimit -Sn); printf '%s %s\n' \"\$hard\" \"\$soft\"" 2>&1)
if [ $? -eq 0 ]; then
    set -- $HARD_KEYWORD_OUT
    if [ $# -eq 2 ] && [ "$1" = "$2" ]; then
        pass_test "ulimit -Sn hard matches hard limit"
    else
        fail_test "ulimit -Sn hard mismatch (got '$HARD_KEYWORD_OUT')"
    fi
else
    fail_test "ulimit -Sn hard failed: $HARD_KEYWORD_OUT"
fi

# Setting without -H/-S should update both hard and soft limits
SET_BOTH_OUT=$("$CJSH_PATH" -c "soft=\$(ulimit -Ss); hard=\$(ulimit -Hs); if [ \"\$soft\" = unlimited ]; then if [ \"\$hard\" = unlimited ]; then target=1024; elif [ \"\$hard\" -gt 1 ]; then target=\$((hard - 1)); else target=1; fi; elif [ \"\$soft\" -gt 1 ]; then target=\$((soft - 1)); else target=1; fi; if [ \"\$hard\" != unlimited ] && [ \"\$target\" -gt \"\$hard\" ]; then target=\$hard; fi; ulimit -s \"\$target\" || exit 90; printf '%s %s %s\n' \"\$target\" \"\$(ulimit -Ss)\" \"\$(ulimit -Hs)\"" 2>&1)
if [ $? -eq 0 ]; then
    set -- $SET_BOTH_OUT
    if [ $# -eq 3 ] && [ "$1" = "$2" ] && [ "$1" = "$3" ]; then
        pass_test "ulimit -s updates both soft and hard limits"
    else
        fail_test "ulimit -s did not update both limits (got '$SET_BOTH_OUT')"
    fi
else
    fail_test "ulimit -s failed to set both limits: $SET_BOTH_OUT"
fi

# Soft stack limits above hard should fail instead of clamping
SOFT_ABOVE_HARD_OUT=$("$CJSH_PATH" -c "orig_soft=\$(ulimit -Ss); orig_hard=\$(ulimit -Hs); if [ \"\$orig_soft\" = unlimited ]; then if [ \"\$orig_hard\" = unlimited ]; then target=1024; elif [ \"\$orig_hard\" -gt 2 ]; then target=\$((orig_hard - 1)); else target=1; fi; elif [ \"\$orig_soft\" -gt 2 ]; then target=\$((orig_soft - 1)); else target=1; fi; if [ \"\$orig_hard\" != unlimited ] && [ \"\$target\" -gt \"\$orig_hard\" ]; then target=\$orig_hard; fi; ulimit -s \"\$target\" || exit 90; too_high=\$((target + 1)); if ulimit -Ss \"\$too_high\" >/dev/null 2>&1; then exit 10; fi; after_soft=\$(ulimit -Ss); after_hard=\$(ulimit -Hs); printf '%s %s %s\n' \"\$target\" \"\$after_soft\" \"\$after_hard\"; exit 0" 2>&1)
SOFT_ABOVE_HARD_STATUS=$?
if [ $SOFT_ABOVE_HARD_STATUS -eq 0 ]; then
    set -- $SOFT_ABOVE_HARD_OUT
    if [ $# -eq 3 ] && [ "$1" = "$2" ] && [ "$1" = "$3" ]; then
        pass_test "ulimit -Ss rejects values above the hard limit"
    else
        fail_test "ulimit -Ss altered limits after rejected set (got '$SOFT_ABOVE_HARD_OUT')"
    fi
elif [ $SOFT_ABOVE_HARD_STATUS -eq 10 ]; then
    fail_test "ulimit -Ss accepted a value above a finite hard limit"
elif [ $SOFT_ABOVE_HARD_STATUS -eq 90 ]; then
    pass_test "ulimit -Ss above-hard check skipped (could not establish finite baseline)"
else
    fail_test "ulimit -Ss above-hard check failed unexpectedly: $SOFT_ABOVE_HARD_OUT"
fi

# Hard-only updates should not silently lower soft limits
HARD_ONLY_OUT=$("$CJSH_PATH" -c "orig_soft=\$(ulimit -Ss); orig_hard=\$(ulimit -Hs); if [ \"\$orig_soft\" = unlimited ] || [ \"\$orig_soft\" -le 1 ]; then exit 3; fi; new_hard=\$((orig_soft - 1)); if ulimit -Hs \"\$new_hard\" >/dev/null 2>&1; then exit 10; fi; after_soft=\$(ulimit -Ss); after_hard=\$(ulimit -Hs); printf '%s %s %s %s\n' \"\$orig_soft\" \"\$after_soft\" \"\$orig_hard\" \"\$after_hard\"" 2>&1)
HARD_ONLY_STATUS=$?
if [ $HARD_ONLY_STATUS -eq 0 ]; then
    set -- $HARD_ONLY_OUT
    if [ $# -eq 4 ] && [ "$1" = "$2" ] && [ "$3" = "$4" ]; then
        pass_test "ulimit -Hs preserves soft limit when hard-only set is invalid"
    else
        fail_test "ulimit -Hs changed limits after rejected hard-only set (got '$HARD_ONLY_OUT')"
    fi
elif [ $HARD_ONLY_STATUS -eq 3 ]; then
    pass_test "ulimit -Hs hard-only invalid-set check skipped"
else
    fail_test "ulimit -Hs invalid hard-only set check failed unexpectedly: $HARD_ONLY_OUT"
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

# Optional resource (pipe size) should either work or provide a clear message
ULIMIT_P_OUTPUT=$("$CJSH_PATH" -c "ulimit -p" 2>&1)
ULIMIT_P_STATUS=$?
if [ $ULIMIT_P_STATUS -eq 0 ]; then
    if is_valid_limit "$ULIMIT_P_OUTPUT"; then
        pass_test "ulimit -p reports pipe size limit"
    else
        fail_test "ulimit -p returned unexpected output"
    fi
else
    if echo "$ULIMIT_P_OUTPUT" | grep -qi "not available"; then
        pass_test "ulimit -p gracefully reports unsupported resource"
    else
        fail_test "ulimit -p failure without clear message"
    fi
fi

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
