#!/usr/bin/env sh
# Test all builtin commands comprehensively
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: builtin commands comprehensive..."

# Test echo builtin
OUT=$("$CJSH_PATH" -c "echo 'hello world'")
if [ "$OUT" != "hello world" ]; then
    echo "FAIL: echo builtin (got '$OUT')"
    exit 1
fi

# Test echo with options
OUT=$("$CJSH_PATH" -c "echo -n 'no newline'")
if [ "$OUT" != "no newline" ]; then
    echo "FAIL: echo -n option (got '$OUT')"
    exit 1
fi

# Test printf builtin
OUT=$("$CJSH_PATH" -c "printf '%s %d\n' hello 42")
if [ "$OUT" != "hello 42" ]; then
    echo "FAIL: printf builtin (got '$OUT')"
    exit 1
fi

# Test version command
OUT=$("$CJSH_PATH" -c "version")
if [ -z "$OUT" ]; then
    echo "FAIL: version command should return version info"
    exit 1
fi

# Test help command
OUT=$("$CJSH_PATH" -c "help" 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "FAIL: help command should return help text"
    exit 1
fi

# Test test builtin (conditional expressions)
"$CJSH_PATH" -c "test -f /etc/passwd"
if [ $? -ne 0 ]; then
    echo "FAIL: test builtin file check"
    exit 1
fi

"$CJSH_PATH" -c "test 1 -eq 1"
if [ $? -ne 0 ]; then
    echo "FAIL: test builtin numeric equality"
    exit 1
fi

"$CJSH_PATH" -c "test 'hello' = 'hello'"
if [ $? -ne 0 ]; then
    echo "FAIL: test builtin string equality"
    exit 1
fi

# Test [ builtin (alternative syntax for test)
"$CJSH_PATH" -c "[ -d /tmp ]"
if [ $? -ne 0 ]; then
    echo "FAIL: [ builtin directory check"
    exit 1
fi

# Test eval builtin
OUT=$("$CJSH_PATH" -c "CMD='echo hello'; eval \$CMD")
if [ "$OUT" != "hello" ]; then
    echo "FAIL: eval builtin (got '$OUT')"
    exit 1
fi

echo "PASS"
exit 0
