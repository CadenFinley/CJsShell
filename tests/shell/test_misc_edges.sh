#!/usr/bin/env sh

# test_misc_edges.sh
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
echo "Test: misc edges..."

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

OUT=$("$CJSH_PATH" -c "   echo   hi   # this is a comment")
if [ "$OUT" != "hi" ]; then
  fail_test "comment/whitespace parsing"
  exit 1
else
  pass_test "comment/whitespace parsing"
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
cat > "$TMPDIR/hello.sh" <<'EOF'
#!/usr/bin/env sh
echo hi
EOF
chmod +x "$TMPDIR/hello.sh"
OUT2=$(PATH="$TMPDIR:$PATH" "$CJSH_PATH" -c "hello.sh")
if [ "$OUT2" != "hi" ]; then
  fail_test "PATH resolution (got '$OUT2')"
  exit 1
else
  pass_test "PATH resolution"
fi

"$CJSH_PATH" -c "false | true"
EC1=$?
"$CJSH_PATH" -c "true | false"
EC2=$?
if [ $EC1 -eq 0 ] && [ $EC2 -ne 0 ]; then
  pass_test "pipeline exit status semantics"
else
  fail_test "pipeline exit status semantics differ or unsupported"
  exit 1
fi

echo ""
echo "Misc Edges Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
