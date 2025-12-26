#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: parser ignores keywords inside quoted strings..."

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

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

SCRIPT_PATH="$TMP_DIR/quoted_keywords.sh"

cat <<'EOF' > "$SCRIPT_PATH"
if true; then
    echo "text (for example: 'foo')."
fi
EOF

OUTPUT_FILE="$TMP_DIR/output.txt"
"$CJSH_PATH" "$SCRIPT_PATH" >"$OUTPUT_FILE" 2>&1
STATUS=$?
if [ $STATUS -eq 0 ] && grep -q "text (for example: 'foo')." "$OUTPUT_FILE"; then
    pass_test "quoted keywords script executes"
else
    cat "$OUTPUT_FILE"
    fail_test "quoted keywords script executes (status=$STATUS)"
fi

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
