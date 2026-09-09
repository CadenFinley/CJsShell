#!/usr/bin/env sh

# test_refactor_regressions.sh
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

echo "Test: refactor regression coverage..."

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

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t cjsh_refactor_tests)
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

HISTORY_FILE="$TMP_DIR/history.txt"
cat >"$HISTORY_FILE" <<'EOF'
# metadata line

echo one
echo two
# ignored comment
echo three
EOF

OUT=$(CJSH_HISTORY_FILE="$HISTORY_FILE" "$CJSH_PATH" -c 'history' 2>/dev/null)
if echo "$OUT" | grep -q "echo one" && echo "$OUT" | grep -q "echo two" &&
   echo "$OUT" | grep -q "echo three" && ! echo "$OUT" | grep -q "metadata line"; then
    pass_test "history filters comments and blank lines"
else
    fail_test "history filtering regression (got '$OUT')"
fi

TILDE_HISTORY_HOME="$TMP_DIR/home"
mkdir -p "$TILDE_HISTORY_HOME"
TILDE_HISTORY_FILE="$TILDE_HISTORY_HOME/tilde-history.txt"
cat >"$TILDE_HISTORY_FILE" <<'EOF'
echo tilde history
EOF

OUT=$(cd "$TMP_DIR" && HOME="$TILDE_HISTORY_HOME" CJSH_HISTORY_FILE='~/tilde-history.txt' "$CJSH_PATH" -c 'history' 2>/dev/null)
if echo "$OUT" | grep -q "echo tilde history"; then
    pass_test "history expands leading tilde in CJSH_HISTORY_FILE"
else
    fail_test "history tilde expansion regression (got '$OUT')"
fi

MAN_HOME="$TMP_DIR/man_home"
MAN_BIN_DIR="$MAN_HOME/bin"
MAN_SCRIPT="$MAN_BIN_DIR/fake-man"
MAN_CACHE_FILE="$MAN_HOME/.cache/cjsh/generated_completions/samplecmd.txt"
mkdir -p "$MAN_BIN_DIR"
cat >"$MAN_SCRIPT" <<'EOF'
#!/usr/bin/env sh

while [ $# -gt 0 ]; do
    if [ "$1" = "-P" ]; then
        shift 2
        continue
    fi
    target=$1
    break
done

printf 'NAME\n  %s - tilde summary\n' "$target"
EOF
chmod +x "$MAN_SCRIPT"

STATUS=0
OUT=$(cd "$TMP_DIR" && HOME="$MAN_HOME" CJSH_MAN_PATH='~/bin/fake-man' "$CJSH_PATH" -c 'generate-completions --quiet samplecmd' 2>/dev/null) || STATUS=$?
if [ "$STATUS" = "0" ] && [ -f "$MAN_CACHE_FILE" ] && grep -q "summary: tilde summary" "$MAN_CACHE_FILE"; then
    pass_test "generate-completions expands leading tilde in CJSH_MAN_PATH"
else
    fail_test "CJSH_MAN_PATH tilde expansion regression (status=$STATUS, out='$OUT')"
fi

OUT=$(CJSH_HISTORY_FILE="$HISTORY_FILE" "$CJSH_PATH" -c 'history nope' >/dev/null 2>&1; echo $?)
if [ "$OUT" = "1" ]; then
    pass_test "history rejects invalid count"
else
    fail_test "history invalid count exit code changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'readonly alpha=beta; readonly' 2>/dev/null)
if echo "$OUT" | grep -q "readonly alpha=beta"; then
    pass_test "readonly default listing format"
else
    fail_test "readonly default listing changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c 'readonly alpha=beta; readonly -p' 2>/dev/null)
if echo "$OUT" | grep -q "readonly alpha='beta'"; then
    pass_test "readonly -p listing format"
else
    fail_test "readonly -p listing changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "trap 'echo trap_hit' EXIT; trap -p" 2>/dev/null)
if echo "$OUT" | grep -q "trap -- 'echo trap_hit' EXIT"; then
    pass_test "trap -p listing format"
else
    fail_test "trap -p listing changed (got '$OUT')"
fi

echo ""
# Exercise preparation through external, builtin, and function dispatch, including
# restoration of only the temporary keys while retaining unrelated variable changes.
OUT=$("$CJSH_PATH" --no-config <<'CJSH_PREP'
PREP=original
PREP=one PREP=two sh -c 'printf "%s:" "$PREP"'
printf "%s:" "$PREP"
PREP=temporary command eval 'SIDE_EFFECT=retained'
printf "%s:%s:" "$PREP" "$SIDE_EFFECT"
prepared_function() { printf "%s:" "$PREP"; }
PREP=function prepared_function
printf "%s" "$PREP"
CJSH_PREP
)
if [ "$OUT" = "two:original:original:retained:function:original" ]; then
    pass_test "prepared commands preserve assignment order and scoped variable changes"
else
    fail_test "prepared command assignment behavior changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" --no-config <<'CJSH_PREP'
unset PREP_ABSENT
PREP_ABSENT=temporary command eval 'SIDE_EFFECT=updated'
printf "%s:%s" "${PREP_ABSENT-unset}" "$SIDE_EFFECT"
CJSH_PREP
)
if [ "$OUT" = "unset:updated" ]; then
    pass_test "temporary assignment restoration removes new keys without losing other changes"
else
    fail_test "temporary assignment restoration changed (got '$OUT')"
fi

# Dispatch must not expand arguments while checking whether a command is a function.
OUT=$("$CJSH_PATH" --no-config --no-source --no-history <<'CJSH_EXPAND'
i=0
printf '%s:' "$((i+=1))"
true && printf '%s:' "$((i+=1))"
f() { printf '%s:' "$1"; }
f "$((i+=1))"
TEMP=value f "$((i+=1))"
printf '%s' "$i"
CJSH_EXPAND
)
if [ "$OUT" = "1:2:3:4:4" ]; then
    pass_test "builtin, logical, and function arguments expand exactly once"
else
    fail_test "argument expansion ran more than once (got '$OUT')"
fi

OUT=$(CJSH_HOTSPOT_OUTPUT="$TMP_DIR/expanded-output" "$CJSH_PATH" --no-config --no-source --no-history <<'CJSH_EXPAND'
i=0
printf '%s' "$((i+=1))" >"$CJSH_HOTSPOT_OUTPUT"
printf '%s:' "$i"
cat "$CJSH_HOTSPOT_OUTPUT"
CJSH_EXPAND
)
if [ "$OUT" = "1:1" ]; then
    pass_test "redirected arguments expand exactly once"
else
    fail_test "redirected argument expansion repeated (got '$OUT')"
fi

OUT=$(CJSH_HOTSPOT_OUTPUT="$TMP_DIR/expanded-words" "$CJSH_PATH" --no-config --no-source --no-history <<'CJSH_EXPAND'
set -- a b
printf '[%s]' "x$@y" >"$CJSH_HOTSPOT_OUTPUT"
cat "$CJSH_HOTSPOT_OUTPUT"
printf '[%s]' {a,,c} | cat
CJSH_EXPAND
)
if [ "$OUT" = "[xa][by][a][c]" ]; then
    pass_test "redirected and pipeline commands preserve word expansion semantics"
else
    fail_test "pipeline word expansion changed (got '$OUT')"
fi

OUT=$("$CJSH_PATH" --no-config --no-source --no-history <<'CJSH_SCOPE'
unset HOTSPOT_SCOPE
HOTSPOT_SCOPE=local
HOTSPOT_SCOPE=temporary printf ''
HOTSPOT_SCOPE=temporary printf '' >/dev/null
printf '%s:' "$HOTSPOT_SCOPE"
sh -c 'printf "%s:" "${HOTSPOT_SCOPE-unset}"'
HOTSPOT_PERSIST=kept :
printf '%s' "$HOTSPOT_PERSIST"
CJSH_SCOPE
)
if [ "$OUT" = "local:unset:kept" ]; then
    pass_test "temporary assignments restore shell and process values independently"
else
    fail_test "temporary environment restoration changed (got '$OUT')"
fi

echo "Refactor Regression Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
