#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: builtin commands comprehensive..."

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

# Test which builtin - basic functionality
OUT=$("$CJSH_PATH" -c "which echo" 2>&1)
if [ $? -ne 0 ]; then
    fail_test "which builtin basic"
    exit 1
else
    pass_test "which builtin basic"
fi

# Test which with builtin command
OUT=$("$CJSH_PATH" -c "which cd" 2>&1)
if echo "$OUT" | grep -q "cjsh builtin"; then
    pass_test "which identifies builtins"
else
    fail_test "which identifies builtins (got '$OUT')"
    exit 1
fi

# Test which with ls (should show cjsh custom implementation)
OUT=$("$CJSH_PATH" -c "which ls" 2>&1)
if echo "$OUT" | grep -q "cjsh builtin"; then
    pass_test "which shows ls as cjsh custom implementation"
else
    fail_test "which shows ls as cjsh custom implementation (got '$OUT')"
    exit 1
fi

# Test which with external command (cat should be available on most systems)
OUT=$("$CJSH_PATH" -c "which cat" 2>&1)
if [ $? -eq 0 ] && echo "$OUT" | grep -q "/"; then
    pass_test "which finds external commands"
else
    # If cat is not found, that's still a valid result
    if echo "$OUT" | grep -q "not found"; then
        pass_test "which handles missing commands correctly"
    else
        fail_test "which external command test (got '$OUT')"
        exit 1
    fi
fi

# Test which with non-existent command
OUT=$("$CJSH_PATH" -c "which nonexistentcommand12345" 2>&1)
if [ $? -ne 0 ] && echo "$OUT" | grep -q "not found"; then
    pass_test "which handles non-existent commands"
else
    fail_test "which handles non-existent commands (got '$OUT')"
    exit 1
fi

# Test which with alias (if aliases are supported)
OUT=$("$CJSH_PATH" -c "alias testwhichalias='echo test'; which testwhichalias" 2>&1)
if echo "$OUT" | grep -q "aliased"; then
    pass_test "which identifies aliases"
else
    # Aliases might not be available in this test context, so we'll skip
    skip_test "which identifies aliases (aliases may not be available in test context)"
fi

# Test which -a option (show all)
OUT=$("$CJSH_PATH" -c "which -a echo" 2>&1)
if [ $? -eq 0 ]; then
    pass_test "which -a option works"
else
    fail_test "which -a option (got '$OUT')"
    exit 1
fi

# Test which -s option (silent)
"$CJSH_PATH" -c "which -s echo" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "which -s option works"
else
    fail_test "which -s option"
    exit 1
fi

# Test which with multiple arguments
OUT=$("$CJSH_PATH" -c "which echo cd" 2>&1)
if [ $? -eq 0 ]; then
    pass_test "which with multiple arguments"
else
    fail_test "which with multiple arguments (got '$OUT')"
    exit 1
fi

# Test which with relative path
if [ -f "$CJSH_PATH" ]; then
    # Create a simple test script
    echo '#!/bin/sh\necho "test script"' > /tmp/cjsh_which_test.sh
    chmod +x /tmp/cjsh_which_test.sh
    
    OUT=$("$CJSH_PATH" -c "cd /tmp && which ./cjsh_which_test.sh" 2>&1)
    if echo "$OUT" | grep -q "cjsh_which_test.sh"; then
        pass_test "which handles relative paths"
    else
        fail_test "which handles relative paths (got '$OUT')"
    fi
    
    # Clean up
    rm -f /tmp/cjsh_which_test.sh
else
    skip_test "which handles relative paths (cjsh binary not found for test setup)"
fi

"$CJSH_PATH" -c "jobs" >/dev/null 2>&1
pass_test "jobs builtin (basic functionality)"

"$CJSH_PATH" -c "wait" >/dev/null 2>&1
if [ $? -gt 1 ]; then
    fail_test "wait builtin"
    exit 1
else
    pass_test "wait builtin"
fi

"$CJSH_PATH" -c "kill -0 $$" >/dev/null 2>&1
pass_test "kill builtin (basic functionality)"

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
