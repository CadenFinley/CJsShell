#!/usr/bin/env sh

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: shell script interpreter error output modes..."

FAILURES=0
PASSES=0

pass() {
    echo "PASS: $1"
    PASSES=$((PASSES + 1))
}

fail() {
    echo "FAIL: $1"
    FAILURES=$((FAILURES + 1))
}

TEMP_DIR="$(mktemp -d /tmp/cjsh_error_output.XXXXXX)"
ERROR_SCRIPT="$TEMP_DIR/invalid_script.sh"

cat >"$ERROR_SCRIPT" <<'EOF'
#!/bin/sh
echo "before"
if true; then
    echo "inside"
# missing fi
EOF

NONTTY_OUTPUT="$("$CJSH_PATH" -c "syntax $ERROR_SCRIPT" 2>&1)"
NONTTY_STATUS=$?

TTY_OUTPUT="$(python3 - "$CJSH_PATH" "$ERROR_SCRIPT" <<'PY'
import os
import pty
import sys

cjsh_path = sys.argv[1]
script_path = sys.argv[2]
collected = bytearray()

def reader(fd):
    data = os.read(fd, 1024)
    collected.extend(data)
    return data

status = pty.spawn([cjsh_path, '-c', f'syntax {script_path}'], reader)
try:
    exit_code = os.waitstatus_to_exitcode(status)
except AttributeError:
    exit_code = status >> 8
sys.stdout.write(collected.decode('utf-8', 'ignore'))
sys.exit(exit_code)
PY
)"
TTY_STATUS=$?

strip_ansi() {
    perl -pe 's/\e\[[0-9;]*[A-Za-z]//g'
}

NONTTY_CLEAN=$(printf "%s" "$NONTTY_OUTPUT" | strip_ansi)
TTY_CLEAN=$(printf "%s" "$TTY_OUTPUT" | strip_ansi)

if printf "%s" "$NONTTY_CLEAN" | grep -Eq "^cjsh: [a-z ]*error: \[[A-Z0-9]+\]"; then
    pass "non-tty error uses compact error_out pathway: $NONTTY_CLEAN"
else
    fail "non-tty error missing compact error_out format: $NONTTY_CLEAN"
fi

if printf "%s" "$NONTTY_CLEAN" | grep -q "┌─"; then
    fail "non-tty error unexpectedly used rich formatter: $NONTTY_CLEAN"
else
    pass "non-tty error avoided rich formatter: $NONTTY_CLEAN"
fi

if printf "%s" "$TTY_CLEAN" | head -n1 | grep -q "^┌"; then
    pass "tty error uses rich formatter: $TTY_CLEAN"
else
    fail "tty error missing rich formatter markers: $TTY_CLEAN"
fi

if printf "%s" "$TTY_CLEAN" | head -n1 | grep -q "^cjsh:"; then
    fail "tty error should not use compact error_out prefix: $TTY_CLEAN"
else
    pass "tty error avoided compact error_out prefix: $TTY_CLEAN"
fi

if [ $NONTTY_STATUS -eq 0 ]; then
    fail "non-tty syntax validation unexpectedly succeeded: $NONTTY_CLEAN"
fi

if [ $TTY_STATUS -eq 0 ]; then
    fail "tty syntax validation unexpectedly succeeded: $TTY_CLEAN"
fi

rm -rf "$TEMP_DIR"

echo ""
echo "Summary: $PASSES passed, $FAILURES failed"

if [ $FAILURES -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
