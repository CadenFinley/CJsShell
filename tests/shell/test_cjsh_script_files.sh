#!/usr/bin/env sh

# test_cjsh_script_files.sh
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
