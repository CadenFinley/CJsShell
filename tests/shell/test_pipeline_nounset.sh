#!/usr/bin/env sh

# test_pipeline_nounset.sh
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
echo "Test: pipeline with set -eu and read"

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

PIPELINE_SCRIPT=$(cat <<'EOF'
set -eu
rows=$(mktemp)
trap 'rm -f "$rows"' EXIT
printf "5\ta\n10\tb\n" > "$rows"
TAB=$(printf '\t')
sort -nr "$rows" | while IFS="$TAB" read -r loc path; do
    printf '%s:%s\n' "$loc" "$path"
done
EOF
)

OUTPUT=$("$CJSH_PATH" -c "$PIPELINE_SCRIPT" 2>&1)
EXIT_CODE=$?

EXPECTED_OUTPUT=$(cat <<'EOF'
10:b
5:a
EOF
)

if [ "$EXIT_CODE" -eq 0 ] && [ "$OUTPUT" = "$EXPECTED_OUTPUT" ]; then
    pass_test "set -eu pipeline preserves read variables"
else
    fail_test "set -eu pipeline failed (exit=$EXIT_CODE, output='$OUTPUT')"
fi

echo ""
echo "Pipeline Nounset Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
