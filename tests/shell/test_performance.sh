#!/usr/bin/env sh
# Comprehensive performance and stress tests
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: performance and stress..."

# Test startup time (should be reasonable)
start_time=$(date +%s%N 2>/dev/null || date +%s)
"$CJSH_PATH" -c "true" >/dev/null 2>&1
end_time=$(date +%s%N 2>/dev/null || date +%s)

if [ "$start_time" != "$end_time" ]; then
    # Only check if we can measure nanoseconds
    if echo "$start_time" | grep -q N; then
        echo "NOTE: Startup time measurement not available"
    else
        duration=$((end_time - start_time))
        if [ "$duration" -gt 5 ]; then
            echo "WARNING: Shell startup took more than 5 seconds"
        fi
    fi
fi

# Test many sequential commands
COMMANDS=""
for i in $(seq 1 100); do
    COMMANDS="$COMMANDS echo test$i;"
done
start_time=$(date +%s)
OUT=$("$CJSH_PATH" -c "$COMMANDS" 2>/dev/null)
end_time=$(date +%s)
duration=$((end_time - start_time))

LINES=$(echo "$OUT" | wc -l)
if [ "$LINES" -ne 100 ]; then
    echo "FAIL: sequential commands test (got $LINES lines, expected 100)"
    exit 1
fi

if [ "$duration" -gt 10 ]; then
    echo "WARNING: 100 sequential commands took more than 10 seconds"
fi

# Test deep pipeline
PIPELINE="echo start"
for i in $(seq 1 20); do
    PIPELINE="$PIPELINE | cat"
done
OUT=$("$CJSH_PATH" -c "$PIPELINE" 2>/dev/null)
if [ "$OUT" != "start" ]; then
    echo "FAIL: deep pipeline test (got '$OUT')"
    exit 1
fi

# Test large output handling
OUT=$("$CJSH_PATH" -c "seq 1 1000" 2>/dev/null)
LINES=$(echo "$OUT" | wc -l)
if [ "$LINES" -ne 1000 ]; then
    echo "FAIL: large output test (got $LINES lines)"
    exit 1
fi

# Test many environment variables
ENV_SETUP=""
for i in $(seq 1 50); do
    ENV_SETUP="$ENV_SETUP TEST_VAR_$i=value$i;"
done
OUT=$("$CJSH_PATH" -c "$ENV_SETUP echo \$TEST_VAR_25" 2>/dev/null)
if [ "$OUT" != "value25" ]; then
    echo "FAIL: many environment variables test (got '$OUT')"
    exit 1
fi

# Test command line length limits
# Generate a very long command line
LONG_ECHO="echo"
for i in $(seq 1 500); do
    LONG_ECHO="$LONG_ECHO word$i"
done
OUT=$("$CJSH_PATH" -c "$LONG_ECHO" 2>/dev/null | wc -w)
if [ "$OUT" -ne 500 ]; then
    echo "WARNING: long command line test failed (got $OUT words)"
fi

# Test many aliases
ALIAS_SETUP=""
for i in $(seq 1 50); do
    ALIAS_SETUP="$ALIAS_SETUP alias test$i='echo alias$i';"
done
OUT=$("$CJSH_PATH" -c "$ALIAS_SETUP test25" 2>/dev/null)
if [ "$OUT" != "alias25" ]; then
    echo "WARNING: many aliases test failed (got '$OUT')"
fi

# Test rapid command execution
start_time=$(date +%s)
for i in $(seq 1 5); do
    "$CJSH_PATH" -c "true" >/dev/null 2>&1
done
end_time=$(date +%s)
duration=$((end_time - start_time))

if [ "$duration" -gt 10 ]; then
    echo "WARNING: 5 shell invocations took more than 10 seconds"
fi

# Test memory usage (basic - just ensure it doesn't crash)
MEMORY_TEST=""
for i in $(seq 1 100); do
    MEMORY_TEST="$MEMORY_TEST VAR$i='$(seq 1 100 | tr '\n' ' ')';"
done
"$CJSH_PATH" -c "$MEMORY_TEST echo done" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "WARNING: memory stress test failed"
fi

# Test concurrent execution (if supported)
{
    "$CJSH_PATH" -c "sleep 0.1; echo concurrent1" &
    "$CJSH_PATH" -c "sleep 0.1; echo concurrent2" &
    "$CJSH_PATH" -c "sleep 0.1; echo concurrent3" &
    wait
} >/dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "WARNING: concurrent execution test had issues"
fi

# Test file descriptor limits
FD_TEST=""
for i in $(seq 1 10); do
    FD_TEST="$FD_TEST echo test$i > /tmp/cjsh_test_$i.txt;"
done
FD_TEST="$FD_TEST rm -f /tmp/cjsh_test_*.txt"
"$CJSH_PATH" -c "$FD_TEST" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "WARNING: file descriptor test failed"
fi

echo "PASS"
exit 0
