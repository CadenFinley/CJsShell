#!/usr/bin/env sh
# Test POSIX compliance of trap command
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: POSIX trap command compliance..."

# Test EXIT signal with name
OUT=$("$CJSH_PATH" -c "trap 'echo EXIT_TRAP' EXIT; echo test")
if ! echo "$OUT" | grep -q "EXIT_TRAP"; then
    echo "FAIL: EXIT signal by name"
    exit 1
fi

# Test EXIT signal with number 0
OUT=$("$CJSH_PATH" -c "trap 'echo ZERO_TRAP' 0; echo test")
if ! echo "$OUT" | grep -q "ZERO_TRAP"; then
    echo "FAIL: EXIT signal by number 0"
    exit 1
fi

# Test ERR signal (should be accepted even if not fully implemented)
"$CJSH_PATH" -c "trap 'echo ERROR' ERR" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: ERR signal not accepted"
    exit 1
fi

# Test DEBUG signal (should be accepted even if not fully implemented)
"$CJSH_PATH" -c "trap 'echo DEBUG' DEBUG" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: DEBUG signal not accepted"
    exit 1
fi

# Test RETURN signal (should be accepted even if not fully implemented)
"$CJSH_PATH" -c "trap 'echo RETURN' RETURN" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: RETURN signal not accepted"
    exit 1
fi

# Test trap listing shows special signals correctly
OUT=$("$CJSH_PATH" -c "trap 'echo exit' EXIT; trap 'echo err' ERR; trap")
if ! echo "$OUT" | grep -q "EXIT"; then
    echo "FAIL: trap listing doesn't show EXIT"
    exit 1
fi
if ! echo "$OUT" | grep -q "ERR"; then
    echo "FAIL: trap listing doesn't show ERR"
    exit 1
fi

# Test removing traps with empty string
"$CJSH_PATH" -c "trap 'echo test' EXIT; trap '' EXIT; trap" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: removing trap with empty string"
    exit 1
fi

# Test removing traps with dash
"$CJSH_PATH" -c "trap 'echo test' EXIT; trap - EXIT; trap" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: removing trap with dash"
    exit 1
fi

# Test traditional POSIX signals still work
"$CJSH_PATH" -c "trap 'echo usr1' USR1" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: traditional USR1 signal"
    exit 1
fi

# Test signal numbers still work
"$CJSH_PATH" -c "trap 'echo int' 2" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: signal number 2 (INT)"
    exit 1
fi

echo "PASS"
exit 0
