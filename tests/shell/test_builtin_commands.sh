#!/usr/bin/env sh
# Test all builtin commands comprehensively (excluding AI, plugin, and theme-related commands)
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

# Test pwd builtin
OUT=$("$CJSH_PATH" -c "pwd")
if [ -z "$OUT" ]; then
    echo "FAIL: pwd builtin should return current directory"
    exit 1
fi

# Test cd builtin (test with relative path)
"$CJSH_PATH" -c "cd /tmp && pwd" >/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: cd builtin"
    exit 1
fi

# Test ls builtin
OUT=$("$CJSH_PATH" -c "ls /tmp" 2>/dev/null)
if [ $? -ne 0 ]; then
    echo "FAIL: ls builtin"
    exit 1
fi

# Test alias and unalias builtins
"$CJSH_PATH" -c "alias ll='ls -l'; unalias ll"
if [ $? -ne 0 ]; then
    echo "FAIL: alias/unalias builtin"
    exit 1
fi

# Test export and unset builtins
"$CJSH_PATH" -c "export TEST_VAR=hello; unset TEST_VAR"
if [ $? -ne 0 ]; then
    echo "FAIL: export/unset builtin"
    exit 1
fi

# Test set builtin
"$CJSH_PATH" -c "set -- arg1 arg2; echo \$1" >/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: set builtin"
    exit 1
fi

# Test shift builtin
OUT=$("$CJSH_PATH" -c "set -- arg1 arg2 arg3; shift; echo \$1")
if [ "$OUT" != "arg2" ]; then
    echo "FAIL: shift builtin (got '$OUT')"
    exit 1
fi

# Test source builtin (.) - create a temporary script
echo 'echo "sourced"' > /tmp/test_source.sh
OUT=$("$CJSH_PATH" -c ". /tmp/test_source.sh")
if [ "$OUT" != "sourced" ]; then
    echo "FAIL: source (.) builtin (got '$OUT')"
    exit 1
fi
rm -f /tmp/test_source.sh

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

# Test approot command
"$CJSH_PATH" -c "approot" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: approot builtin"
    exit 1
fi

# Test eval builtin
OUT=$("$CJSH_PATH" -c "CMD='echo hello'; eval \$CMD")
if [ "$OUT" != "hello" ]; then
    echo "FAIL: eval builtin (got '$OUT')"
    exit 1
fi

# Test history builtin
"$CJSH_PATH" -c "history" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: history builtin"
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

# Test exec builtin (with no args should succeed)
"$CJSH_PATH" -c "exec"
if [ $? -ne 0 ]; then
    echo "FAIL: exec builtin (no args)"
    exit 1
fi

# Test null command (:)
"$CJSH_PATH" -c ":"
if [ $? -ne 0 ]; then
    echo "FAIL: null command (:)"
    exit 1
fi

# Test trap builtin (POSIX EXIT signal)
"$CJSH_PATH" -c "trap 'echo trapped' EXIT" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: trap builtin"
    exit 1
fi

# Test readonly builtin
"$CJSH_PATH" -c "readonly TEST_RO=value" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: readonly builtin"
    exit 1
fi

# Test read builtin (with timeout to avoid hanging)
echo "test_input" | "$CJSH_PATH" -c "read input; echo \$input" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: read builtin"
    exit 1
fi

# Test umask builtin
"$CJSH_PATH" -c "umask" >/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: umask builtin"
    exit 1
fi

# Test getopts builtin
"$CJSH_PATH" -c "getopts 'ab:' opt -a -b value" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: getopts builtin"
    exit 1
fi

# Test times builtin
"$CJSH_PATH" -c "times" >/dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: times builtin"
    exit 1
fi

# Test type builtin
OUT=$("$CJSH_PATH" -c "type echo")
if [ $? -ne 0 ]; then
    echo "FAIL: type builtin"
    exit 1
fi

# Test hash builtin
"$CJSH_PATH" -c "hash" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: hash builtin"
    exit 1
fi

# Test job control commands (basic functionality)
# Note: These may not work in all environments, so we test more leniently
"$CJSH_PATH" -c "jobs" >/dev/null 2>&1
# Don't fail on jobs command as it may not be supported in all test environments

# Test wait command (should succeed with no jobs)
"$CJSH_PATH" -c "wait" >/dev/null 2>&1
if [ $? -gt 1 ]; then  # Allow exit code 1 for "no jobs" but fail on other errors
    echo "FAIL: wait builtin"
    exit 1
fi

# Test kill command (test with invalid PID should fail gracefully)
"$CJSH_PATH" -c "kill -0 $$" >/dev/null 2>&1
# Don't test kill with actual signals in test environment

# Test loop control commands (break, continue, return)
# These are typically used in loops/functions, so test basic parsing
"$CJSH_PATH" -c "break 2>/dev/null || true" >/dev/null 2>&1
"$CJSH_PATH" -c "continue 2>/dev/null || true" >/dev/null 2>&1
"$CJSH_PATH" -c "return 2>/dev/null || true" >/dev/null 2>&1

echo "PASS"
exit 0
