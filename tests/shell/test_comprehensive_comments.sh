#!/usr/bin/env sh
# Test comment and here document handling
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: comment and here document handling..."

# Test 1: Basic comment handling
OUT=$("$CJSH_PATH" -c "echo 'test' # this is a comment")
if [ "$OUT" != "test" ]; then
    echo "FAIL: basic comment handling (got '$OUT')"
    exit 1
fi

# Test 2: Comment with quotes
OUT=$("$CJSH_PATH" -c "echo 'test with # hash inside quotes' # comment")
if [ "$OUT" != "test with # hash inside quotes" ]; then
    echo "FAIL: comment with quotes (got '$OUT')"
    exit 1
fi

# Test 3: Comment with escaped quotes
OUT=$("$CJSH_PATH" -c "echo \"test with \\\"escaped quote\\\"\" # comment")
if [ "$OUT" != "test with \"escaped quote\"" ]; then
    echo "FAIL: comment with escaped quotes (got '$OUT')"
    exit 1
fi

# Test 4: Multiple hashes in content vs comment
OUT=$("$CJSH_PATH" -c "echo 'Multiple ## hashes ### test' # Comment with ## multiple hashes")
if [ "$OUT" != "Multiple ## hashes ### test" ]; then
    echo "FAIL: multiple hashes handling (got '$OUT')"
    exit 1
fi

# Test 5: Empty comment
OUT=$("$CJSH_PATH" -c "echo 'Empty comment test' #")
if [ "$OUT" != "Empty comment test" ]; then
    echo "FAIL: empty comment (got '$OUT')"
    exit 1
fi

# Test 6: Comment with spaces before hash
OUT=$("$CJSH_PATH" -c "echo 'Spaces before comment'        # lots of spaces")
if [ "$OUT" != "Spaces before comment" ]; then
    echo "FAIL: comment with spaces (got '$OUT')"
    exit 1
fi

# Test 7: Hash in variable expansion
OUT=$("$CJSH_PATH" -c "VAR='value#with#hash'; echo \"Variable: \$VAR\" # should print: value#with#hash")
if [ "$OUT" != "Variable: value#with#hash" ]; then
    echo "FAIL: hash in variable expansion (got '$OUT')"
    exit 1
fi

# Test 8: Brace expansion with hash
OUT=$("$CJSH_PATH" -c "VAR='value#with#hash'; echo \"\${VAR}#suffix\" # should print: value#with#hash#suffix")
if [ "$OUT" != "value#with#hash#suffix" ]; then
    echo "FAIL: brace expansion with hash (got '$OUT')"
    exit 1
fi

# Test 9: Basic here document
OUT=$("$CJSH_PATH" -c "cat << 'EOF'
This is a basic here document
with multiple lines
EOF")
EXPECTED="This is a basic here document
with multiple lines"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: basic here document (got '$OUT')"
    exit 1
fi

# Test 10: Here document with variable expansion (unquoted delimiter)
OUT=$("$CJSH_PATH" -c "USER='testuser'; cat << EOF
Hello \$USER!
EOF")
EXPECTED="Hello testuser!"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: here document with variable expansion (got '$OUT')"
    exit 1
fi

echo "PASS"
exit 0
