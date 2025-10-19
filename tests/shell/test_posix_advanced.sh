#!/usr/bin/env sh
# Test counters
TOTAL=0
PASSED=0
FAILED=0

# Shell to test
SHELL_TO_TEST="${1:-./build/cjsh}"

log_test() {
    TOTAL=$((TOTAL + 1))
    printf "Test %03d: %s... " "$TOTAL" "$1"
}

pass() {
    PASSED=$((PASSED + 1))
    printf "${GREEN}PASS${NC}\n"
}

fail() {
    FAILED=$((FAILED + 1))
    printf "${RED}FAIL${NC} - %s\n" "$1"
}

# Check if shell exists
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Advanced POSIX compliance for: $SHELL_TO_TEST"
echo "====================================================="

# Test 1: Parameter expansion - basic
log_test "Parameter expansion \${var}"
result=$("$SHELL_TO_TEST" -c "var=test; echo \${var}" 2>/dev/null)
if [ "$result" = "test" ]; then
    pass
else
    fail "Expected 'test', got '$result'"
fi

# Test 2: Parameter expansion with default value
log_test "Parameter expansion \${var:-default}"
result=$("$SHELL_TO_TEST" -c "echo \${UNDEFINED_VAR:-default}" 2>/dev/null)
if [ "$result" = "default" ]; then
    pass
else
    fail "Expected 'default', got '$result'"
fi

# Test 3: Parameter expansion with assignment
log_test "Parameter expansion \${var:=default}"
result=$("$SHELL_TO_TEST" -c "echo \${UNDEFINED_VAR2:=assigned}; echo \$UNDEFINED_VAR2" 2>/dev/null)
expected="assigned
assigned"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Parameter assignment expansion failed"
fi

# Test 4: Parameter expansion with error
log_test "Parameter expansion \${var:?error}"
"$SHELL_TO_TEST" -c "echo \${UNDEFINED_VAR3:?undefined variable}" 2>/dev/null
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    fail "Should have failed with undefined variable error"
fi

# Test 5: Parameter expansion - string length
log_test "Parameter expansion \${#var}"
result=$("$SHELL_TO_TEST" -c "var=hello; echo \${#var}" 2>/dev/null)
if [ "$result" = "5" ]; then
    pass
else
    fail "Expected '5', got '$result'"
fi

# Test 6: Here document with variable expansion
log_test "Here document with variable expansion"
result=$("$SHELL_TO_TEST" -c "var=world; cat << EOF
Hello \$var
EOF" 2>/dev/null)
if [ "$result" = "Hello world" ]; then
    pass
else
    fail "Here document variable expansion failed"
fi

# Test 7: Here document with quoted delimiter (no expansion)
log_test "Here document with quoted delimiter"
result=$("$SHELL_TO_TEST" -c "var=world; cat << 'EOF'
Hello \$var
EOF" 2>/dev/null)
if [ "$result" = "Hello \$var" ]; then
    pass
else
    fail "Here document should not expand variables with quoted delimiter"
fi

# Test 8: Here string (<<<) if supported
# log_test "Here string (<<<)"
# result=$("$SHELL_TO_TEST" -c "cat <<< 'hello world'" 2>/dev/null)
# if [ "$result" = "hello world" ]; then
#     pass
# else
#     # Here strings are bash extension, not POSIX - mark as warning
#     printf "${YELLOW}SKIP${NC} - Here strings are bash extension, not POSIX\n"
# fi

# Test 9: Case statement
log_test "Case statement"
result=$("$SHELL_TO_TEST" -c "var=apple; case \$var in apple) echo fruit;; *) echo other;; esac" 2>/dev/null)
if [ "$result" = "fruit" ]; then
    pass
else
    fail "Case statement failed, got '$result'"
fi

# Test 10: Case statement with patterns
log_test "Case statement with patterns"
result=$("$SHELL_TO_TEST" -c "var=apple; case \$var in a*) echo starts_with_a;; *) echo other;; esac" 2>/dev/null)
if [ "$result" = "starts_with_a" ]; then
    pass
else
    fail "Case statement pattern matching failed"
fi

# Test 11: For loop with list
log_test "For loop with list"
result=$("$SHELL_TO_TEST" -c "for i in 1 2 3; do echo \$i; done" 2>/dev/null)
expected="1
2
3"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "For loop with list failed"
fi

# Test 12: For loop with glob expansion
log_test "For loop with glob expansion"
# Create test files
mkdir -p "/tmp/posix_test_$$"
touch "/tmp/posix_test_$$/file1.txt" "/tmp/posix_test_$$/file2.txt"
result=$("$SHELL_TO_TEST" -c "cd /tmp/posix_test_$$; for f in *.txt; do echo \$f; done | sort" 2>/dev/null)
expected="file1.txt
file2.txt"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "For loop with glob expansion failed"
fi
rm -rf "/tmp/posix_test_$$"

# Test 13: While loop
log_test "While loop"
result=$("$SHELL_TO_TEST" -c "i=1; while [ \$i -le 3 ]; do echo \$i; i=\$((i+1)); done" 2>/dev/null)
expected="1
2
3"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "While loop failed"
fi

# Test 14: Until loop
log_test "Until loop"
result=$("$SHELL_TO_TEST" -c "i=1; until [ \$i -gt 3 ]; do echo \$i; i=\$((i+1)); done" 2>/dev/null)
expected="1
2
3"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Until loop failed"
fi

# Test 15: If-then-else statement
log_test "If-then-else statement"
result=$("$SHELL_TO_TEST" -c "if [ 1 -eq 1 ]; then echo true; else echo false; fi" 2>/dev/null)
if [ "$result" = "true" ]; then
    pass
else
    fail "If-then-else failed"
fi

# Test 16: If-then-elif-else statement
log_test "If-then-elif-else statement"
result=$("$SHELL_TO_TEST" -c "x=2; if [ \$x -eq 1 ]; then echo one; elif [ \$x -eq 2 ]; then echo two; else echo other; fi" 2>/dev/null)
if [ "$result" = "two" ]; then
    pass
else
    fail "If-then-elif-else failed"
fi

# Test 17: Arithmetic expansion
log_test "Arithmetic expansion \$((...))"
result=$("$SHELL_TO_TEST" -c "echo \$((2 + 3))" 2>/dev/null)
if [ "$result" = "5" ]; then
    pass
else
    fail "Arithmetic expansion failed, got '$result'"
fi

# Test 18: Arithmetic expansion with variables
log_test "Arithmetic expansion with variables"
result=$("$SHELL_TO_TEST" -c "a=5; b=3; echo \$((a * b))" 2>/dev/null)
if [ "$result" = "15" ]; then
    pass
else
    fail "Arithmetic expansion with variables failed"
fi

# Test 19: Function definition and call
log_test "Function definition and call"
result=$("$SHELL_TO_TEST" -c "myfunc() { echo hello \$1; }; myfunc world" 2>/dev/null)
if [ "$result" = "hello world" ]; then
    pass
else
    fail "Function definition/call failed"
fi

# Test 20: Function with return value
log_test "Function with return value"
"$SHELL_TO_TEST" -c "myfunc() { return 42; }; myfunc" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 42 ]; then
    pass
else
    fail "Function return value failed, got exit code $exit_code"
fi

# Test 21: Local variables in function (if supported)
log_test "Function local variables"
result=$("$SHELL_TO_TEST" -c "var=global; myfunc() { local var=local; echo \$var; }; myfunc; echo \$var" 2>/dev/null)
expected="local
global"
if [ "$result" = "$expected" ]; then
    pass
else
    # Local is not strictly POSIX
    printf "${YELLOW}SKIP${NC} - 'local' is not strictly POSIX\n"
fi

# Test 22: Command grouping with { }
log_test "Command grouping with { }"
result=$("$SHELL_TO_TEST" -c "{ echo first; echo second; }" 2>/dev/null)
expected="first
second"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Command grouping with braces failed"
fi

# Test 23: Subshell with ( )
log_test "Subshell with ( )"
result=$("$SHELL_TO_TEST" -c "var=outer; (var=inner; echo \$var); echo \$var" 2>/dev/null)
expected="inner
outer"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Subshell variable isolation failed"
fi

# Test 24: Complex pipeline
log_test "Complex pipeline"
result=$("$SHELL_TO_TEST" -c "echo 'hello world' | tr ' ' '\n' | wc -l" 2>/dev/null | tr -d ' ')
if [ "$result" = "2" ]; then
    pass
else
    fail "Complex pipeline failed, got '$result'"
fi

# Test 25: File descriptor redirection
log_test "File descriptor redirection"
"$SHELL_TO_TEST" -c "echo output; echo error >&2" > "/tmp/stdout_$$" 2> "/tmp/stderr_$$"
stdout_content=$(cat "/tmp/stdout_$$" 2>/dev/null)
stderr_content=$(cat "/tmp/stderr_$$" 2>/dev/null)
if [ "$stdout_content" = "output" ] && [ "$stderr_content" = "error" ]; then
    pass
else
    fail "File descriptor redirection failed"
fi
rm -f "/tmp/stdout_$$" "/tmp/stderr_$$"

# Test 26: Process substitution (if supported)
# log_test "Process substitution"
# result=$("$SHELL_TO_TEST" -c "diff <(echo hello) <(echo hello)" 2>/dev/null)
# exit_code=$?
# if [ $exit_code -eq 0 ]; then
#     pass
# else
#     # Process substitution is bash extension
#     printf "${YELLOW}SKIP${NC} - Process substitution is bash extension, not POSIX\n"
# fi

# Test 27: Word splitting
log_test "Word splitting"
result=$("$SHELL_TO_TEST" -c "set -- a b c; echo \$#" 2>/dev/null)
if [ "$result" = "3" ]; then
    pass
else
    fail "Word splitting failed, got '$result'"
fi

# Test 28: Pathname expansion
log_test "Pathname expansion"
mkdir -p "/tmp/glob_test_$$"
touch "/tmp/glob_test_$$/test1.txt" "/tmp/glob_test_$$/test2.txt"
result=$("$SHELL_TO_TEST" -c "cd /tmp/glob_test_$$; echo *.txt | wc -w" 2>/dev/null | tr -d ' ')
if [ "$result" = "2" ]; then
    pass
else
    fail "Pathname expansion failed"
fi
rm -rf "/tmp/glob_test_$$"

# Test 29: Exit status of pipeline
log_test "Exit status of pipeline"
"$SHELL_TO_TEST" -c "false | true" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass
else
    fail "Pipeline should return exit status of last command (true=0)"
fi

# Test 30: Set built-in with options
log_test "Set builtin with options"
result=$("$SHELL_TO_TEST" -c "set -e; echo test" 2>/dev/null)
if [ "$result" = "test" ]; then
    pass
else
    fail "Set builtin failed"
fi

echo "====================================================="
echo "Advanced POSIX Compliance Test Results:"
echo "Total tests: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All advanced tests passed!${NC}"
    exit 0
else
    echo "${YELLOW}Some advanced tests failed. Review the failures above.${NC}"
    echo "Success rate: $(( PASSED * 100 / TOTAL ))%"
    exit 1
fi
