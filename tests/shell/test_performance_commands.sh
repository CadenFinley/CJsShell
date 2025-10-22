#!/usr/bin/env sh
# Test suite for performance benchmark commands
# This validates all commands used in time_test_binaries.py

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: Performance benchmark commands validation..."

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

# Test 1: ls command
OUT=$("$CJSH_PATH" -c "ls" 2>&1)
if [ $? -eq 0 ]; then
    pass_test "ls - directory listing"
else
    fail_test "ls - directory listing (exit code $?)"
fi

# Test 2: version banner
OUT=$("$CJSH_PATH" --version 2>&1)
if [ $? -eq 0 ] && [ -n "$OUT" ]; then
    pass_test "version banner"
else
    fail_test "version banner"
fi

# Test 3: hello world string
OUT=$("$CJSH_PATH" -c 'echo hello world')
if [ "$OUT" = "hello world" ]; then
    pass_test "hello world string"
else
    fail_test "hello world string (got '$OUT')"
fi

# Test 4: pwd command
OUT=$("$CJSH_PATH" -c "pwd")
if [ $? -eq 0 ] && [ -n "$OUT" ]; then
    pass_test "pwd - current working directory"
else
    fail_test "pwd - current working directory"
fi

# Test 5: date expansion
OUT=$("$CJSH_PATH" -c 'echo $(date)')
if [ $? -eq 0 ] && [ -n "$OUT" ]; then
    pass_test "date expansion"
else
    fail_test "date expansion"
fi

# Test 6: shell variable expansion
OUT=$("$CJSH_PATH" -c 'echo $SHELL')
if [ $? -eq 0 ]; then
    pass_test "shell variable expansion"
else
    fail_test "shell variable expansion"
fi

# Test 7: ls with long format
OUT=$("$CJSH_PATH" -c 'ls -la' 2>&1)
if [ $? -eq 0 ]; then
    pass_test "ls -la long format"
else
    fail_test "ls -la long format"
fi

# Test 8: exit command
OUT=$("$CJSH_PATH" -c 'exit')
if [ $? -eq 0 ]; then
    pass_test "exit command"
else
    fail_test "exit command (exit code $?)"
fi

# Test 9: high-iteration loop (5000 iterations)
echo "Running 5000-iteration loop test (this may take a moment)..."
OUT=$("$CJSH_PATH" -c 'for i in {1..5000}; do echo $i; done' 2>&1)
LINE_COUNT=$(echo "$OUT" | wc -l | tr -d ' ')
if [ "$LINE_COUNT" = "5000" ]; then
    pass_test "5000-iteration loop"
else
    fail_test "5000-iteration loop (expected 5000 lines, got $LINE_COUNT)"
fi

# Test 10: loop with conditional filtering (even numbers)
echo "Running loop with conditional filtering test..."
OUT=$("$CJSH_PATH" -c 'for i in {1..5000}; do if [ $((i % 2)) -eq 0 ]; then echo $i; fi; done' 2>&1)
LINE_COUNT=$(echo "$OUT" | wc -l | tr -d ' ')
if [ "$LINE_COUNT" = "2500" ]; then
    pass_test "loop with conditional filtering (even numbers)"
else
    fail_test "loop with conditional filtering (expected 2500 lines, got $LINE_COUNT)"
fi

# Test 11: branching with nested conditionals
echo "Running nested conditionals test..."
OUT=$("$CJSH_PATH" -c 'count=0; for i in {1..2000}; do if [ $((i % 15)) -eq 0 ]; then count=$((count+1)); elif [ $((i % 3)) -eq 0 ]; then :; elif [ $((i % 5)) -eq 0 ]; then :; fi; done; echo $count' 2>&1)
if [ "$OUT" = "133" ]; then
    pass_test "nested conditionals with arithmetic (count divisible by 15)"
else
    fail_test "nested conditionals with arithmetic (expected 133, got '$OUT')"
fi

# Test 12: function definition and repeated calls
echo "Running function call test..."
OUT=$("$CJSH_PATH" -c 'sum(){ out=0; for n in "$@"; do out=$((out+n)); done; echo "$out"; }; for i in {1..400}; do sum 1 2 3 4 5 >/dev/null; done; echo "done"' 2>&1)
if [ $? -eq 0 ] && echo "$OUT" | grep -q "done"; then
    pass_test "function definition and repeated calls (400 iterations)"
else
    fail_test "function definition and repeated calls"
fi

# Test 13: subshell directory traversal
echo "Running subshell traversal test..."
OUT=$("$CJSH_PATH" -c 'for dir in /bin /usr/bin /usr/sbin; do if [ -d "$dir" ]; then (cd "$dir" && ls >/dev/null); fi; done' 2>&1)
if [ $? -eq 0 ]; then
    pass_test "subshell directory traversal"
else
    fail_test "subshell directory traversal"
fi

# Test 14: verify arithmetic in loops works correctly
OUT=$("$CJSH_PATH" -c 'sum=0; for i in {1..100}; do sum=$((sum + i)); done; echo $sum')
if [ "$OUT" = "5050" ]; then
    pass_test "arithmetic accumulation in loop (sum 1-100)"
else
    fail_test "arithmetic accumulation in loop (expected 5050, got '$OUT')"
fi

# Test 15: command substitution with multiple levels
OUT=$("$CJSH_PATH" -c 'echo $(echo $(echo nested))')
if [ "$OUT" = "nested" ]; then
    pass_test "nested command substitution"
else
    fail_test "nested command substitution (got '$OUT')"
fi

# Test 16: brace expansion in loops
OUT=$("$CJSH_PATH" -c 'for i in {1..10}; do echo $i; done' | wc -l | tr -d ' ')
if [ "$OUT" = "10" ]; then
    pass_test "brace expansion {1..10}"
else
    fail_test "brace expansion {1..10} (expected 10 lines, got $OUT)"
fi

# Test 17: pipeline with multiple commands
OUT=$("$CJSH_PATH" -c 'echo -e "line1\nline2\nline3" | wc -l' 2>&1 | tr -d ' ')
if [ "$OUT" = "3" ] || [ "$OUT" = "1" ]; then
    pass_test "pipeline with wc"
else
    fail_test "pipeline with wc (got '$OUT')"
fi

# Test 18: variable assignment and expansion
OUT=$("$CJSH_PATH" -c 'VAR=42; echo $VAR')
if [ "$OUT" = "42" ]; then
    pass_test "variable assignment and expansion"
else
    fail_test "variable assignment and expansion (got '$OUT')"
fi

# Test 19: test builtin with various conditions
OUT=$("$CJSH_PATH" -c 'if [ 5 -gt 3 ]; then echo yes; else echo no; fi')
if [ "$OUT" = "yes" ]; then
    pass_test "test builtin with -gt operator"
else
    fail_test "test builtin with -gt operator (got '$OUT')"
fi

# Test 20: string comparison
OUT=$("$CJSH_PATH" -c 'if [ "abc" = "abc" ]; then echo match; fi')
if [ "$OUT" = "match" ]; then
    pass_test "string comparison with = operator"
else
    fail_test "string comparison with = operator (got '$OUT')"
fi

# Test 21: modulo arithmetic
OUT=$("$CJSH_PATH" -c 'echo $((10 % 3))')
if [ "$OUT" = "1" ]; then
    pass_test "modulo arithmetic operator"
else
    fail_test "modulo arithmetic operator (got '$OUT')"
fi

# Test 22: complex arithmetic expression
OUT=$("$CJSH_PATH" -c 'echo $(((5 + 3) * 2 - 4))')
if [ "$OUT" = "12" ]; then
    pass_test "complex arithmetic expression"
else
    fail_test "complex arithmetic expression (got '$OUT')"
fi

# Test 23: null command (:) in branches
OUT=$("$CJSH_PATH" -c 'if true; then :; fi; echo done')
if [ "$OUT" = "done" ]; then
    pass_test "null command (:) in conditional"
else
    fail_test "null command (:) in conditional (got '$OUT')"
fi

# Test 24: brace expansion with arithmetic counting
OUT=$("$CJSH_PATH" -c 'count=0; for i in {1..5}; do count=$((count+1)); done; echo $count')
if [ "$OUT" = "5" ]; then
    pass_test "brace expansion with counter in loop"
else
    fail_test "brace expansion with counter in loop (expected 5, got '$OUT')"
fi

# Test 25: parameter expansion with default value
OUT=$("$CJSH_PATH" -c 'echo ${UNDEFINED_VAR:-default}')
if [ "$OUT" = "default" ]; then
    pass_test "parameter expansion with default value"
else
    fail_test "parameter expansion with default value (got '$OUT')"
fi

# Print summary
echo ""
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "  SKIPPED: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
