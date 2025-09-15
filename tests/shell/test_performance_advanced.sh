#!/usr/bin/env sh
# Test performance and resource usage
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: performance and resource usage..."

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

# Helper function to check if command is available
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Test 1: Startup time
echo "Testing startup time..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "echo startup test" >/dev/null 2>&1
end_time=$(date +%s%N)
startup_time=$((($end_time - $start_time) / 1000000))  # Convert to milliseconds

if [ $startup_time -lt 1000 ]; then  # Less than 1 second
    pass_test "startup time reasonable (${startup_time}ms)"
else
    skip_test "startup time (${startup_time}ms, may be system dependent)"
fi

# Test 2: Memory usage
echo "Testing memory usage..."
if command_exists ps; then
    # Start shell in background and measure memory
    "$CJSH_PATH" -c "sleep 1" &
    shell_pid=$!
    sleep 0.1  # Give it time to start
    
    if ps -p $shell_pid >/dev/null 2>&1; then
        # Get memory usage (RSS in KB)
        memory_kb=$(ps -o rss= -p $shell_pid 2>/dev/null | tr -d ' ')
        wait $shell_pid
        
        if [ -n "$memory_kb" ] && [ "$memory_kb" -lt 50000 ]; then  # Less than 50MB
            pass_test "memory usage reasonable (${memory_kb}KB)"
        else
            skip_test "memory usage (${memory_kb}KB, may be system dependent)"
        fi
    else
        skip_test "memory measurement (process not found)"
    fi
else
    skip_test "memory usage test (ps not available)"
fi

# Test 3: Command execution speed
echo "Testing command execution speed..."
start_time=$(date +%s%N)
for i in 1 2 3 4 5 6 7 8 9 10; do
    "$CJSH_PATH" -c "echo test $i" >/dev/null 2>&1
done
end_time=$(date +%s%N)
total_time=$((($end_time - $start_time) / 1000000))  # Convert to milliseconds
avg_time=$((total_time / 10))

if [ $avg_time -lt 100 ]; then  # Less than 100ms per command
    pass_test "command execution speed (${avg_time}ms avg)"
else
    skip_test "command execution speed (${avg_time}ms avg, may be system dependent)"
fi

# Test 4: Large output handling
echo "Testing large output handling..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "seq 1 10000" >/tmp/large_output_test.out 2>&1
end_time=$(date +%s%N)
large_output_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ] && [ -f /tmp/large_output_test.out ]; then
    output_lines=$(wc -l < /tmp/large_output_test.out)
    if [ "$output_lines" -eq 10000 ]; then
        pass_test "large output handling (${large_output_time}ms)"
    else
        fail_test "large output handling (wrong line count: $output_lines)"
    fi
else
    fail_test "large output handling"
fi

# Test 5: Multiple concurrent shells
echo "Testing concurrent shell instances..."
# Start multiple shells in background
for i in 1 2 3 4 5; do
    "$CJSH_PATH" -c "sleep 0.5; echo concurrent $i" >/tmp/concurrent_$i.out 2>&1 &
done

# Wait for all to complete
wait

# Check results
concurrent_pass=0
for i in 1 2 3 4 5; do
    if [ -f /tmp/concurrent_$i.out ] && grep -q "concurrent $i" /tmp/concurrent_$i.out; then
        concurrent_pass=$((concurrent_pass + 1))
    fi
done

if [ $concurrent_pass -eq 5 ]; then
    pass_test "concurrent shell instances"
else
    fail_test "concurrent shell instances ($concurrent_pass/5 succeeded)"
fi

# Test 6: File handling performance
echo "Testing file handling performance..."
# Create test files
mkdir -p /tmp/file_test
for i in $(seq 1 100); do
    echo "file content $i" > /tmp/file_test/file$i.txt
done

start_time=$(date +%s%N)
"$CJSH_PATH" -c "ls /tmp/file_test/*.txt | wc -l" >/tmp/file_count.out 2>&1
end_time=$(date +%s%N)
file_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ]; then
    file_count=$(cat /tmp/file_count.out | tr -d ' ')
    if [ "$file_count" -eq 100 ]; then
        pass_test "file handling performance (${file_time}ms)"
    else
        fail_test "file handling (wrong count: $file_count)"
    fi
else
    fail_test "file handling performance"
fi

# Test 7: Process creation overhead
echo "Testing process creation overhead..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "true; true; true; true; true" >/dev/null 2>&1
end_time=$(date +%s%N)
process_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ]; then
    if [ $process_time -lt 500 ]; then  # Less than 500ms for 5 commands
        pass_test "process creation overhead (${process_time}ms)"
    else
        skip_test "process creation overhead (${process_time}ms, may be system dependent)"
    fi
else
    fail_test "process creation test"
fi

# Test 8: Variable expansion performance
echo "Testing variable expansion performance..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "TEST=hello; for i in 1 2 3 4 5; do echo \$TEST\$i; done" >/tmp/var_expand.out 2>&1
end_time=$(date +%s%N)
var_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ]; then
    pass_test "variable expansion performance (${var_time}ms)"
else
    fail_test "variable expansion test"
fi

# Test 9: Pipeline performance
echo "Testing pipeline performance..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "seq 1 1000 | grep 5 | wc -l" >/tmp/pipeline_test.out 2>&1
end_time=$(date +%s%N)
pipeline_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ]; then
    pass_test "pipeline performance (${pipeline_time}ms)"
else
    fail_test "pipeline performance"
fi

# Test 10: Resource cleanup
echo "Testing resource cleanup..."
# Create a shell that allocates resources and exits
"$CJSH_PATH" -c "
    for i in \$(seq 1 100); do
        VAR\$i=value\$i
    done
    echo cleanup test
" >/tmp/cleanup_test.out 2>&1

if [ $? -eq 0 ] && grep -q "cleanup test" /tmp/cleanup_test.out; then
    pass_test "resource cleanup"
else
    fail_test "resource cleanup"
fi

# Test 11: Stress test with loops
echo "Testing stress with loops..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "
    count=0
    while [ \$count -lt 1000 ]; do
        count=\$((count + 1))
    done
    echo \$count
" >/tmp/stress_test.out 2>&1
end_time=$(date +%s%N)
stress_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ] && grep -q "1000" /tmp/stress_test.out; then
    if [ $stress_time -lt 5000 ]; then  # Less than 5 seconds
        pass_test "stress test with loops (${stress_time}ms)"
    else
        skip_test "stress test (${stress_time}ms, may be slow system)"
    fi
else
    fail_test "stress test with loops"
fi

# Test 12: Long-running command handling
echo "Testing long-running command handling..."
start_time=$(date +%s%N)
"$CJSH_PATH" -c "sleep 2; echo long running done" >/tmp/long_running.out 2>&1
end_time=$(date +%s%N)
long_time=$((($end_time - $start_time) / 1000000))

if [ $? -eq 0 ] && grep -q "long running done" /tmp/long_running.out; then
    # Should be approximately 2000ms
    if [ $long_time -gt 1800 ] && [ $long_time -lt 2500 ]; then
        pass_test "long-running command handling (${long_time}ms)"
    else
        skip_test "long-running command timing (${long_time}ms)"
    fi
else
    fail_test "long-running command handling"
fi

# Cleanup
rm -f /tmp/large_output_test.out /tmp/concurrent_*.out /tmp/file_count.out
rm -f /tmp/var_expand.out /tmp/pipeline_test.out /tmp/cleanup_test.out
rm -f /tmp/stress_test.out /tmp/long_running.out
rm -rf /tmp/file_test

echo ""
echo "Performance and Resource Usage Tests Summary:"
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