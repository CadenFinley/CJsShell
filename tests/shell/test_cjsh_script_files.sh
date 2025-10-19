#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: .cjsh files behave as regular scripts..."
TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM
SCRIPT_PATH="$TMP_DIR/test_script.cjsh"
cat <<'EOF' > "$SCRIPT_PATH"
echo "script executed"
EOF
OUT=$("$CJSH_PATH" -c "source \"$SCRIPT_PATH\"")
if [ "$OUT" != "script executed" ]; then
    echo "FAIL: sourcing .cjsh script (got '$OUT')"
    exit 1
else
    echo "PASS: sourcing .cjsh script"
fi
OUT=$("$CJSH_PATH" "$SCRIPT_PATH")
if [ "$OUT" != "script executed" ]; then
    echo "FAIL: executing .cjsh script directly (got '$OUT')"
    exit 1
else
    echo "PASS: executing .cjsh script directly"
fi
echo "PASS"
exit 0
