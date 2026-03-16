#!/usr/bin/env sh

# test_line_continuations.sh
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

# Verifies that cjsh preserves POSIX line-continuation semantics

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

if [ ! -x "$CJSH_PATH" ]; then
    echo "FAIL: cjsh binary not found at $CJSH_PATH"
    exit 1
fi

echo "Test: line continuations"

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo "PASS: $1"
}

fail_test() {
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo "FAIL: $1"
    exit 1
}

# 1. Basic continuation in -c scripts
OUT=$("$CJSH_PATH" -c 'printf "%s" "foo" \
"bar"')
if [ "$OUT" = "foobar" ]; then
    pass_test "basic printf continuation"
else
    fail_test "basic printf continuation (got '$OUT')"
fi

# 2. Continuations tolerate trailing whitespace after the backslash
OUT=$("$CJSH_PATH" -c 'printf "%s" "left" \    
"right"')
if [ "$OUT" = "leftright" ]; then
    pass_test "continuation with trailing spaces"
else
    fail_test "continuation with trailing spaces (got '$OUT')"
fi

# 3. Continuations inside script files (mirrors regression case)
TMP_SCRIPT=$(mktemp)
cat <<'EOF' >"$TMP_SCRIPT"
#!/usr/bin/env cjsh
set -eu
PROGRESS_STEP=2
count=0
while [ "${count}" -lt "${PROGRESS_STEP}" ]; do
    printf 'tick %d/%d' \
        "${count}" "${PROGRESS_STEP}"
    printf '\n'
    count=$((count + 1))
done
EOF
chmod +x "$TMP_SCRIPT"
FILE_OUT=$("$CJSH_PATH" "$TMP_SCRIPT")
rm -f "$TMP_SCRIPT"
EXPECTED=$(cat <<'EOF'
tick 0/2
tick 1/2
EOF
)
if [ "$FILE_OUT" = "$EXPECTED" ]; then
    pass_test "continuation in script file"
else
    fail_test "continuation in script file (got '$FILE_OUT')"
fi

echo ""
echo "Line Continuation Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
