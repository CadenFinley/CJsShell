#!/usr/bin/env sh

# test_pipeline_while_conditionals.sh
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

if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: pipeline while loops inside conditionals"

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

PIPELINE_SCRIPT='run_block() {
    if printf "%s\n" "warn" | grep -q "warn"; then
        data=$(printf "%s\n" warn info)
        if printf "%s\n" "$data" | grep -q "warn"; then
            printf "%s\n" "$data" | while IFS= read -r line; do
                if [ -n "$line" ]; then
                    echo "line:$line"
                fi
            done
        fi
    fi
}

run_block
'

OUTPUT=$("$CJSH_PATH" -c "$PIPELINE_SCRIPT" 2>&1)
EXIT_CODE=$?

if [ "$EXIT_CODE" -eq 0 ]; then
    echo "$OUTPUT" | grep -q "line:warn"
    HAS_WARN=$?
    echo "$OUTPUT" | grep -q "line:info"
    HAS_INFO=$?
    echo "$OUTPUT" | grep -q "SYN001"
    HAS_SYNTAX=$?
    if [ "$HAS_WARN" -eq 0 ] && [ "$HAS_INFO" -eq 0 ] && [ "$HAS_SYNTAX" -ne 0 ]; then
        pass_test "pipeline while loop inside nested conditionals executes"
    else
        fail_test "pipeline while loop inside nested conditionals should run successfully"
        echo "$OUTPUT"
    fi
else
    fail_test "pipeline while loop script exited with $EXIT_CODE"
    echo "$OUTPUT"
fi

INLINE_SCRIPT='for i in 1 2 3; do printf "%s " "$i"; done'
INLINE_OUTPUT=$("$CJSH_PATH" -c "$INLINE_SCRIPT" 2>&1)
INLINE_EXIT=$?

if [ "$INLINE_EXIT" -eq 0 ] && [ "$INLINE_OUTPUT" = "1 2 3 " ]; then
    pass_test "inline for loop closes correctly"
else
    fail_test "inline for loop should produce '1 2 3 '"
    echo "$INLINE_OUTPUT"
fi

echo ""
echo "Pipeline While Conditional Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
