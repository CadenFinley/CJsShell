#!/usr/bin/env sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: cjsh invocation behavior..."

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

TMP_ROOT=$(mktemp -d 2>/dev/null)
if [ -z "$TMP_ROOT" ]; then
    echo "Unable to create temporary directory"
    exit 1
fi
trap 'rm -rf "$TMP_ROOT"' EXIT

zero_probe="$TMP_ROOT/zero_probe.sh"
cat <<'EOF' > "$zero_probe"
#!/usr/bin/env sh
script_dir=$(cd "$(dirname "$0")" && pwd)
printf "%s|%s\n" "$0" "$script_dir"
EOF
chmod +x "$zero_probe"

OUT=$("$CJSH_PATH" "$zero_probe")
CANON_TMP=$(cd "$TMP_ROOT" && pwd -P)
SCRIPT_ZERO=${OUT%%|*}
SCRIPT_DIR=${OUT#*|}
if [ "$SCRIPT_ZERO" = "$zero_probe" ] && [ "$SCRIPT_DIR" = "$CANON_TMP" ]; then
    pass_test "script invoked via cjsh preserves \$0 and dirname"
else
    if [ "$SCRIPT_ZERO" != "$zero_probe" ]; then
        fail_test "\$0 mismatch (expected '$zero_probe', got '$SCRIPT_ZERO')"
    else
        fail_test "dirname mismatch (expected '$CANON_TMP', got '$SCRIPT_DIR')"
    fi
fi

dirname_probe="$TMP_ROOT/dirname_probe.sh"
cat <<'EOF' > "$dirname_probe"
#!/usr/bin/env sh
TARGET="$1"
/usr/bin/dirname "$TARGET"
EOF
chmod +x "$dirname_probe"

TARGET_PATH="$TMP_ROOT/nested/example/script.sh"
mkdir -p "$(dirname "$TARGET_PATH")"
OUT=$("$CJSH_PATH" "$dirname_probe" "$TARGET_PATH")
EXPECTED=$(/usr/bin/dirname "$TARGET_PATH")
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "external command arguments include unexported shell variables"
else
    fail_test "external command arguments include unexported shell variables (got '$OUT')"
fi

noexec_marker="$TMP_ROOT/noexec_marker"
"$CJSH_PATH" --no-exec -c "touch \"$noexec_marker\"" >/dev/null 2>&1
if [ ! -e "$noexec_marker" ]; then
    pass_test "--no-exec skips command execution"
else
    fail_test "--no-exec should skip command execution"
fi

echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))
if [ $TESTS_FAILED -eq 0 ]; then
    echo "All tests passed! ($TESTS_PASSED/$TOTAL_TESTS)"
    exit 0
else
    echo "Some tests failed. ($TESTS_PASSED/$TOTAL_TESTS)"
    exit 1
fi
