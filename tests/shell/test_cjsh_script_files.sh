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

SOURCE_PARAMS_PATH="$TMP_DIR/source_params.cjsh"
cat <<'EOF' > "$SOURCE_PARAMS_PATH"
printf "%s|%s|%s|%s\n" "$0" "$#" "$1" "$2"
EOF

OUT=$("$CJSH_PATH" -c "set -- one two; source \"$SOURCE_PARAMS_PATH\"")
if [ "$OUT" != "$CJSH_PATH|2|one|two" ]; then
    echo "FAIL: source preserves caller parameters (got '$OUT')"
    exit 1
else
    echo "PASS: source preserves caller parameters"
fi

OUT=$("$CJSH_PATH" -c "set -- one two; . \"$SOURCE_PARAMS_PATH\"")
if [ "$OUT" != "$CJSH_PATH|2|one|two" ]; then
    echo "FAIL: dot preserves caller parameters (got '$OUT')"
    exit 1
else
    echo "PASS: dot preserves caller parameters"
fi

COMMAND_SCRIPT_PATH="$TMP_DIR/command_dispatch.cjsh"
cat <<'EOF' > "$COMMAND_SCRIPT_PATH"
echo "command dispatch ok"
EOF

OUT=$("$CJSH_PATH" -c "\"$COMMAND_SCRIPT_PATH\"" 2>&1)
if [ "$OUT" != "command dispatch ok" ]; then
    echo "FAIL: executing .cjsh command inside cjsh (got '$OUT')"
    exit 1
else
    echo "PASS: executing .cjsh command inside cjsh"
fi

SHEBANG_SCRIPT_PATH="$TMP_DIR/shebang_dispatch"
cat <<'EOF' > "$SHEBANG_SCRIPT_PATH"
#!/usr/bin/env cjsh
echo "shebang dispatch ok"
EOF

OUT=$("$CJSH_PATH" -c "\"$SHEBANG_SCRIPT_PATH\"" 2>&1)
if [ "$OUT" != "shebang dispatch ok" ]; then
    echo "FAIL: cjsh shebang dispatch inside cjsh (got '$OUT')"
    exit 1
else
    echo "PASS: cjsh shebang dispatch inside cjsh"
fi

FALSE_POSITIVE_PATH="$TMP_DIR/not_really_cjsh.txt"
cat <<'EOF' > "$FALSE_POSITIVE_PATH"
echo "false positive cjsh marker"
EOF

OUT=$("$CJSH_PATH" -c "\"$FALSE_POSITIVE_PATH\"" 2>&1)
STATUS=$?
if [ $STATUS -eq 126 ] && [ "$OUT" != "false positive cjsh marker" ]; then
    echo "PASS: plain files mentioning cjsh are not misdetected as cjsh scripts"
else
    echo "FAIL: plain files mentioning cjsh should not run internally (status=$STATUS, got '$OUT')"
    exit 1
fi

echo "PASS"
exit 0
