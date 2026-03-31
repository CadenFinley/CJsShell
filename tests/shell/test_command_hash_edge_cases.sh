#!/usr/bin/env sh

# test_command_hash_edge_cases.sh
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

echo "Test: command/hash uncovered edge cases..."

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

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

cat > "$TMP_DIR/mycmd" <<'EOF'
#!/bin/sh
printf 'external\n'
EOF
chmod +x "$TMP_DIR/mycmd"

cat > "$TMP_DIR/-dashcmd" <<'EOF'
#!/bin/sh
printf 'dashcmd\n'
EOF
chmod +x "$TMP_DIR/-dashcmd"

OUT=$("$CJSH_PATH" -c "command" 2>&1)
STATUS=$?
if [ $STATUS -eq 2 ] && echo "$OUT" | grep -q "usage:"; then
    pass_test "command shows usage with no args"
else
    fail_test "command no-args usage (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "command -z echo" 2>&1)
STATUS=$?
if [ $STATUS -eq 2 ] && echo "$OUT" | grep -q "invalid option"; then
    pass_test "command rejects invalid options"
else
    fail_test "command invalid option handling (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "command -v echo" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "echo" ]; then
    pass_test "command -v reports builtin name"
else
    fail_test "command -v builtin output (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "command -V echo" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && echo "$OUT" | grep -q "echo is a shell builtin"; then
    pass_test "command -V reports builtin details"
else
    fail_test "command -V builtin details (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "command -v definitely_missing_command_for_test" 2>&1)
STATUS=$?
if [ $STATUS -eq 1 ] && [ -z "$OUT" ]; then
    pass_test "command -v missing command is silent"
else
    fail_test "command -v missing command behavior (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "command -V definitely_missing_command_for_test" 2>&1)
STATUS=$?
if [ $STATUS -eq 1 ] && echo "$OUT" | grep -q "not found"; then
    pass_test "command -V missing command reports error"
else
    fail_test "command -V missing command behavior (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "PATH=\"$TMP_DIR:\$PATH\"; mycmd() { echo function; }; mycmd; command mycmd")
EXPECTED="function
external"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "command bypasses shell functions"
else
    fail_test "command function bypass (out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "onlyfunc() { echo function; }; command onlyfunc" 2>&1)
STATUS=$?
if [ $STATUS -ne 0 ] && ! echo "$OUT" | grep -q "^function$"; then
    pass_test "command does not execute function-only names"
else
    fail_test "command should not invoke function-only names (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "PATH='/definitely/not/real'; command -p sh -c 'printf alive'; printf '|%s' \"\$PATH\"")
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "alive|/definitely/not/real" ]; then
    pass_test "command -p uses default PATH and restores PATH"
else
    fail_test "command -p PATH restoration (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "PATH='/definitely/not/real'; command -p -v sh" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && { [ "$OUT" = "/bin/sh" ] || [ "$OUT" = "/usr/bin/sh" ]; }; then
    pass_test "command -p -v resolves with default PATH"
else
    fail_test "command -p -v resolution (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "PATH=\"$TMP_DIR:\$PATH\"; command -- -dashcmd" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && [ "$OUT" = "dashcmd" ]; then
    pass_test "command -- handles dash-prefixed executable names"
else
    fail_test "command -- dash-prefixed names (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "hash /bin/sh" 2>&1)
STATUS=$?
if [ $STATUS -eq 1 ] && echo "$OUT" | grep -q "Only bare command names"; then
    pass_test "hash rejects names containing '/'"
else
    fail_test "hash slash-name rejection (status=$STATUS, out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "hash -r; hash sh missing_hash_command_for_test; status=\$?; hash; echo status:\$status" 2>&1)
if echo "$OUT" | grep -q "status:1" &&
   { echo "$OUT" | grep -q "/bin/sh" || echo "$OUT" | grep -q "/usr/bin/sh"; }; then
    pass_test "hash reports mixed success/failure correctly"
else
    fail_test "hash mixed outcomes (out='$OUT')"
fi

OUT=$("$CJSH_PATH" -c "hash sh; hash -r; hash" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] && echo "$OUT" | grep -q "no cached commands"; then
    pass_test "hash -r clears cache entries"
else
    fail_test "hash -r cache clearing (status=$STATUS, out='$OUT')"
fi

echo ""
echo "Command/Hash Edge Case Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
