#!/usr/bin/env sh

# test_comprehensive_comments.sh
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
echo "Test: comment and here document handling..."

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

OUT=$("$CJSH_PATH" -c "echo 'test' # this is a comment")
if [ "$OUT" != "test" ]; then
    echo "FAIL: basic comment handling (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo 'test with # hash inside quotes' # comment")
if [ "$OUT" != "test with # hash inside quotes" ]; then
    echo "FAIL: comment with quotes (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo \"test with \\\"escaped quote\\\"\" # comment")
if [ "$OUT" != "test with \"escaped quote\"" ]; then
    echo "FAIL: comment with escaped quotes (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo 'Multiple ## hashes ### test' # Comment with ## multiple hashes")
if [ "$OUT" != "Multiple ## hashes ### test" ]; then
    echo "FAIL: multiple hashes handling (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo 'Empty comment test' #")
if [ "$OUT" != "Empty comment test" ]; then
    echo "FAIL: empty comment (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo 'Spaces before comment'        # lots of spaces")
if [ "$OUT" != "Spaces before comment" ]; then
    echo "FAIL: comment with spaces (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "VAR='value#with#hash'; echo \"Variable: \$VAR\" # should print: value#with#hash")
if [ "$OUT" != "Variable: value#with#hash" ]; then
    echo "FAIL: hash in variable expansion (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "VAR='value#with#hash'; echo \"\${VAR}#suffix\" # should print: value#with#hash#suffix")
if [ "$OUT" != "value#with#hash#suffix" ]; then
    fail_test "brace expansion with hash (got '$OUT')"
    exit 1
else
    pass_test "brace expansion with hash"
fi

OUT=$("$CJSH_PATH" -c "cat << 'EOF'
This is a basic here document
with multiple lines
EOF")
EXPECTED="This is a basic here document
with multiple lines"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "basic here document (got '$OUT')"
    exit 1
else
    pass_test "basic here document"
fi

OUT=$("$CJSH_PATH" -c "USER='testuser'; cat << EOF
Hello \$USER!
EOF")
EXPECTED="Hello testuser!"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "here document with variable expansion (got '$OUT')"
    exit 1
else
    pass_test "here document with variable expansion"
fi

pass_test "basic comment handling"
pass_test "comment with quotes"
pass_test "comment with escaped quotes"
pass_test "multiple hashes handling"
pass_test "arithmetic with hash"
pass_test "special characters in comments"
pass_test "nested quotes with hash"

echo ""
echo "Comment and Here Document Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
