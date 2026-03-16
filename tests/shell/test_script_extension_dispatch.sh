#!/usr/bin/env sh

# test_script_extension_dispatch.sh
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
