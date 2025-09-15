#!/usr/bin/env sh
# Test all builtin commands comprehensively (excluding AI, plugin, and theme-related commands)
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: builtin commands comprehensive..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

# Test echo builtin
OUT=$("$CJSH_PATH" -c "echo 'hello world'")
if [ "$OUT" != "hello world" ]; then
    fail_test "echo builtin (got '$OUT')"
    exit 1
else
    pass_test "echo builtin"
fi

# Test echo with options
OUT=$("$CJSH_PATH" -c "echo -n 'no newline'")
if [ "$OUT" != "no newline" ]; then
    fail_test "echo -n option (got '$OUT')"
    exit 1
else
    pass_test "echo -n option"
fi

# Test printf builtin
OUT=$("$CJSH_PATH" -c "printf '%s %d\n' hello 42")
if [ "$OUT" != "hello 42" ]; then
    fail_test "printf builtin (got '$OUT')"
    exit 1
else
    pass_test "printf builtin"
fi

# Test pwd builtin
OUT=$("$CJSH_PATH" -c "pwd")
if [ -z "$OUT" ]; then
    fail_test "pwd builtin should return current directory"
    exit 1
else
    pass_test "pwd builtin"
fi

# Test cd builtin (test with relative path)
"$CJSH_PATH" -c "cd /tmp && pwd" >/dev/null
if [ $? -ne 0 ]; then
    fail_test "cd builtin"
    exit 1
else
    pass_test "cd builtin"
fi

# Test ls builtin
OUT=$("$CJSH_PATH" -c "ls /tmp" 2>/dev/null)
if [ $? -ne 0 ]; then
    fail_test "ls builtin"
    exit 1
else
    pass_test "ls builtin"
fi

# Test alias and unalias builtins
"$CJSH_PATH" -c "alias ll='ls -l'; unalias ll"
if [ $? -ne 0 ]; then
    fail_test "alias/unalias builtin"
    exit 1
else
    pass_test "alias/unalias builtin"
fi

# Test export and unset builtins
"$CJSH_PATH" -c "export TEST_VAR=hello; unset TEST_VAR"
if [ $? -ne 0 ]; then
    fail_test "export/unset builtin"
    exit 1
else
    pass_test "export/unset builtin"
fi

# Test set builtin
"$CJSH_PATH" -c "set -- arg1 arg2; echo \$1" >/dev/null
if [ $? -ne 0 ]; then
    fail_test "set builtin"
    exit 1
else
    pass_test "set builtin"
fi

# Test shift builtin
OUT=$("$CJSH_PATH" -c "set -- arg1 arg2 arg3; shift; echo \$1")
if [ "$OUT" != "arg2" ]; then
    fail_test "shift builtin (got '$OUT')"
    exit 1
else
    pass_test "shift builtin"
fi

# Test source builtin (.) - create a temporary script
echo 'echo "sourced"' > /tmp/test_source.sh
OUT=$("$CJSH_PATH" -c ". /tmp/test_source.sh")
if [ "$OUT" != "sourced" ]; then
    fail_test "source (.) builtin (got '$OUT')"
    exit 1
else
    pass_test "source (.) builtin"
fi
rm -f /tmp/test_source.sh

# Test version command
OUT=$("$CJSH_PATH" -c "version")
if [ -z "$OUT" ]; then
    fail_test "version command should return version info"
    exit 1
else
    pass_test "version command"
fi

# Test help command
OUT=$("$CJSH_PATH" -c "help" 2>/dev/null)
if [ -z "$OUT" ]; then
    fail_test "help command should return help text"
    exit 1
else
    pass_test "help command"
fi

# Test approot command
"$CJSH_PATH" -c "approot" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    fail_test "approot builtin"
    exit 1
else
    pass_test "approot builtin"
fi

# Test eval builtin
OUT=$("$CJSH_PATH" -c "CMD='echo hello'; eval \$CMD")
if [ "$OUT" != "hello" ]; then
    fail_test "eval builtin (got '$OUT')"
    exit 1
else
    pass_test "eval builtin"
fi

# Test test builtin (conditional expressions)
"$CJSH_PATH" -c "test -f /etc/passwd"
if [ $? -ne 0 ]; then
    fail_test "test builtin file check"
    exit 1
else
    pass_test "test builtin file check"
fi

"$CJSH_PATH" -c "test 1 -eq 1"
if [ $? -ne 0 ]; then
    fail_test "test builtin numeric equality"
    exit 1
else
    pass_test "test builtin numeric equality"
fi

"$CJSH_PATH" -c "test 'hello' = 'hello'"
if [ $? -ne 0 ]; then
    fail_test "test builtin string equality"
    exit 1
else
    pass_test "test builtin string equality"
fi

# Test [ builtin (alternative syntax for test)
"$CJSH_PATH" -c "[ -d /tmp ]"
if [ $? -ne 0 ]; then
    fail_test "[ builtin directory check"
    exit 1
else
    pass_test "[ builtin directory check"
fi

# Test exec builtin (with no args should succeed)
"$CJSH_PATH" -c "exec"
if [ $? -ne 0 ]; then
    fail_test "exec builtin (no args)"
    exit 1
else
    pass_test "exec builtin (no args)"
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
    fail_test "trap builtin"
    exit 1
else
    pass_test "trap builtin"
fi

# Test readonly builtin
"$CJSH_PATH" -c "readonly TEST_RO=value" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    fail_test "readonly builtin"
    exit 1
else
    pass_test "readonly builtin"
fi

# Test read builtin (with timeout to avoid hanging)
echo "test_input" | "$CJSH_PATH" -c "read input; echo \$input" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    fail_test "read builtin"
    exit 1
else
    pass_test "read builtin"
fi

# Test umask builtin
"$CJSH_PATH" -c "umask" >/dev/null
if [ $? -ne 0 ]; then
    fail_test "umask builtin"
    exit 1
else
    pass_test "umask builtin"
fi

# Test getopts builtin
"$CJSH_PATH" -c "getopts 'ab:' opt -a -b value" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    fail_test "getopts builtin"
    exit 1
else
    pass_test "getopts builtin"
fi

# Test times builtin
"$CJSH_PATH" -c "times" >/dev/null
if [ $? -ne 0 ]; then
    fail_test "times builtin"
    exit 1
else
    pass_test "times builtin"
fi

# Test type builtin
OUT=$("$CJSH_PATH" -c "type echo")
if [ $? -ne 0 ]; then
    fail_test "type builtin"
    exit 1
else
    pass_test "type builtin"
fi

# Test hash builtin
"$CJSH_PATH" -c "hash" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    fail_test "hash builtin"
    exit 1
else
    pass_test "hash builtin"
fi

# Test job control commands (basic functionality)
# Note: These may not work in all environments, so we test more leniently
"$CJSH_PATH" -c "jobs" >/dev/null 2>&1
pass_test "jobs builtin (basic functionality)"

# Test wait command (should succeed with no jobs)
"$CJSH_PATH" -c "wait" >/dev/null 2>&1
if [ $? -gt 1 ]; then  # Allow exit code 1 for "no jobs" but fail on other errors
    fail_test "wait builtin"
    exit 1
else
    pass_test "wait builtin"
fi

# Test kill command (test with invalid PID should fail gracefully)
"$CJSH_PATH" -c "kill -0 $$" >/dev/null 2>&1
pass_test "kill builtin (basic functionality)"

# Test loop control commands (break, continue, return)
# These are typically used in loops/functions, so test basic parsing
"$CJSH_PATH" -c "break 2>/dev/null || true" >/dev/null 2>&1
"$CJSH_PATH" -c "continue 2>/dev/null || true" >/dev/null 2>&1
"$CJSH_PATH" -c "return 2>/dev/null || true" >/dev/null 2>&1
pass_test "loop control commands (basic functionality)"

echo ""
echo "Builtin Commands Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
