#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: script extension dispatch..."

TMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

SCRIPT_SH="$TMP_DIR/test_no_shebang.sh"
cat <<'EOF' > "$SCRIPT_SH"
echo "sh extension ok"
EOF
chmod +x "$SCRIPT_SH"

SCRIPT_SH_NOEXEC="$TMP_DIR/test_no_shebang_noexec.sh"
cat <<'EOF' > "$SCRIPT_SH_NOEXEC"
echo "sh extension noexec ok"
EOF

OUT=$("$CJSH_PATH" -c "\"$SCRIPT_SH\"")
if [ "$OUT" != "sh extension ok" ]; then
    echo "FAIL: .sh without shebang uses sh (got '$OUT')"
    exit 1
else
    echo "PASS: .sh without shebang uses sh"
fi

OUT=$("$CJSH_PATH" --no-script-extension-interpreter -c "\"$SCRIPT_SH_NOEXEC\"" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 126 ]; then
    echo "FAIL: extension dispatch disabled should fail (exit $EXIT_CODE)"
    exit 1
else
    echo "PASS: extension dispatch can be disabled"
fi

if command -v bash >/dev/null 2>&1; then
    SCRIPT_BASH="$TMP_DIR/test_no_shebang.bash"
    cat <<'EOF' > "$SCRIPT_BASH"
if [[ 1 -eq 1 ]]; then
    echo "bash extension ok"
fi
EOF
    chmod +x "$SCRIPT_BASH"

    OUT=$("$CJSH_PATH" -c "\"$SCRIPT_BASH\"")
    if [ "$OUT" != "bash extension ok" ]; then
        echo "FAIL: .bash without shebang uses bash (got '$OUT')"
        exit 1
    else
        echo "PASS: .bash without shebang uses bash"
    fi

    SCRIPT_SHEBANG="$TMP_DIR/test_shebang.sh"
    cat <<'EOF' > "$SCRIPT_SHEBANG"
#!/usr/bin/env bash
if [[ 2 -eq 2 ]]; then
    echo "shebang ok"
fi
EOF
    chmod +x "$SCRIPT_SHEBANG"

    OUT=$("$CJSH_PATH" -c "\"$SCRIPT_SHEBANG\"")
    if [ "$OUT" != "shebang ok" ]; then
        echo "FAIL: shebang takes precedence (got '$OUT')"
        exit 1
    else
        echo "PASS: shebang takes precedence"
    fi
else
    echo "SKIP: bash not found"
fi

echo "PASS"
exit 0
