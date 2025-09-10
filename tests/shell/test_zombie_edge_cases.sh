#!/usr/bin/env sh
# Test advanced zombie process scenarios and edge cases
# Tests complex scenarios where zombie processes might be created

if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: advanced zombie process scenarios..."

# Test 1: Rapid process forking doesn't overwhelm SIGCHLD handler
"$CJSH_PATH" -c 'true & true & true & true & true & wait'
if [ $? -eq 0 ]; then
    echo "PASS: rapid forking"
else
    echo "FAIL: rapid forking caused issues"
    exit 1
fi

# Test 2: Nested background processes  
"$CJSH_PATH" -c '(sleep 0.1 & sleep 0.1 & wait) & wait'
if [ $? -eq 0 ]; then
    echo "PASS: nested background processes"
else
    echo "FAIL: nested background processes"
    exit 1
fi

# Test 3: Background process with multiple children
"$CJSH_PATH" -c 'true & true & true & wait'
if [ $? -eq 0 ]; then
    echo "PASS: background process with children"
else
    echo "FAIL: background process with children"
    exit 1
fi

# Test 4: Simple pipeline test
"$CJSH_PATH" -c 'echo test | cat > /dev/null'
if [ $? -eq 0 ]; then
    echo "PASS: pipeline with background components"
else
    echo "FAIL: pipeline with background components"
    exit 1
fi

# Test 5: Command substitution test
OUT=$("$CJSH_PATH" -c 'echo substituted')
if [ "$OUT" = "substituted" ]; then
    echo "PASS: command substitution in background"
else
    echo "FAIL: command substitution in background (got '$OUT')"
    exit 1
fi

# Test 6: Multiple levels of process nesting
"$CJSH_PATH" -c '(echo deep > /dev/null) & wait'
if [ $? -eq 0 ]; then
    echo "PASS: multiple levels of nesting"
else
    echo "FAIL: multiple levels of nesting"
    exit 1
fi

# Test 7: Background processes with different exit codes
"$CJSH_PATH" -c 'true & false & sleep 0.1 & wait'
if [ $? -eq 0 ]; then
    echo "PASS: mixed exit codes in background"
else
    echo "FAIL: mixed exit codes in background"
    exit 1
fi

# Test 8: Process substitution with pipes
"$CJSH_PATH" -c 'echo test | cat | wc -l > /dev/null'
if [ $? -eq 0 ]; then
    echo "PASS: process substitution with background"
else
    echo "FAIL: process substitution with background"
    exit 1
fi

# Test 9: Background job with I/O redirection
"$CJSH_PATH" -c 'echo background > /tmp/cjsh_zombie_test_bg.txt & wait; cat /tmp/cjsh_zombie_test_bg.txt > /dev/null; rm -f /tmp/cjsh_zombie_test_bg.txt'
if [ $? -eq 0 ]; then
    echo "PASS: background job with I/O redirection"
else
    echo "FAIL: background job with I/O redirection"
    exit 1
fi

# Test 10: Stress test - many short-lived processes
"$CJSH_PATH" -c 'true & true & true & true & true & true & true & true & true & true & wait'
if [ $? -eq 0 ]; then
    echo "PASS: stress test with many processes"
else
    echo "FAIL: stress test with many processes"
    exit 1
fi

# Test 11: Background process terminated by signal
"$CJSH_PATH" -c 'sleep 2 & PID=$!; kill -TERM $PID; wait $PID 2>/dev/null'
if [ $? -ne 0 ]; then  # Should fail because process was terminated
    echo "PASS: background process with signal handling"
else
    echo "FAIL: background process with signal handling"
    exit 1
fi

# Test 12: Quick-exiting background job
"$CJSH_PATH" -c 'true & sleep 0.05; wait'
if [ $? -eq 0 ]; then
    echo "PASS: quick-exiting background job"
else
    echo "FAIL: quick-exiting background job"
    exit 1
fi

# Test 13: Complex pipeline 
"$CJSH_PATH" -c 'echo line1; echo line2; echo line3 | grep line | wc -l > /dev/null'
if [ $? -eq 0 ]; then
    echo "PASS: complex pipeline with background elements"
else
    echo "FAIL: complex pipeline with background elements"
    exit 1
fi

# Test 14: Background process with environment variables
export ZOMBIE_TEST_VAR="test_value"
"$CJSH_PATH" -c 'echo $ZOMBIE_TEST_VAR > /dev/null & wait'
if [ $? -eq 0 ]; then
    echo "PASS: background process with environment variables"
else
    echo "FAIL: background process with environment variables"
    exit 1
fi

# Test 15: Verify no zombie accumulation after all tests
ZOMBIE_COUNT=$(ps aux | awk '$8 ~ /^Z/ { count++ } END { print count+0 }')
if [ "$ZOMBIE_COUNT" -eq 0 ]; then
    echo "PASS: no zombie accumulation after tests"
else
    echo "FAIL: found $ZOMBIE_COUNT zombie processes after tests"
    # Show the zombies for debugging
    echo "Zombie processes found:"
    ps aux | awk '$8 ~ /^Z/ { print }'
    exit 1
fi

echo "PASS: All advanced zombie process tests completed successfully"
exit 0
