#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: performance and stress..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

if date +%s%N >/dev/null 2>&1; then
    start_time=$(date +%s%N)
    "$CJSH_PATH" -c "true" >/dev/null 2>&1
    end_time=$(date +%s%N)
    duration_ns=$((end_time - start_time))
    duration_sec=$((duration_ns / 1000000000))
    if [ "$duration_sec" -gt 5 ]; then
        fail_test "Shell startup took more than 5 seconds ($duration_sec seconds)"
    else
        pass_test "Shell startup time"
    fi
else
    start_time=$(date +%s)
    "$CJSH_PATH" -c "true" >/dev/null 2>&1
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    if [ "$duration" -gt 5 ]; then
        fail_test "Shell startup took more than 5 seconds ($duration seconds)"
    else
        pass_test "Shell startup time"
    fi
fi

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
    fail_test "sequential commands test (got $LINES lines, expected 100)"
else
    pass_test "sequential commands execution"
fi

if [ "$duration" -gt 10 ]; then
    fail_test "100 sequential commands took more than 10 seconds"
else
    pass_test "sequential commands performance"
fi

PIPELINE="echo start"
for i in $(seq 1 20); do
    PIPELINE="$PIPELINE | cat"
done
OUT=$("$CJSH_PATH" -c "$PIPELINE" 2>/dev/null)
if [ "$OUT" != "start" ]; then
    fail_test "deep pipeline test (got '$OUT')"
else
    pass_test "deep pipeline execution"
fi

OUT=$("$CJSH_PATH" -c "seq 1 1000" 2>/dev/null)
LINES=$(echo "$OUT" | wc -l)
if [ "$LINES" -ne 1000 ]; then
    fail_test "large output test (got $LINES lines)"
else
    pass_test "large output handling"
fi

ENV_SETUP=""
for i in $(seq 1 50); do
    ENV_SETUP="$ENV_SETUP TEST_VAR_$i=value$i;"
done
OUT=$("$CJSH_PATH" -c "$ENV_SETUP echo \$TEST_VAR_25" 2>/dev/null)
if [ "$OUT" != "value25" ]; then
    fail_test "many environment variables test (got '$OUT')"
else
    pass_test "many environment variables handling"
fi

LONG_ECHO="echo"
for i in $(seq 1 500); do
    LONG_ECHO="$LONG_ECHO word$i"
done
OUT=$("$CJSH_PATH" -c "$LONG_ECHO" 2>/dev/null | wc -w)
if [ "$OUT" -ne 500 ]; then
    fail_test "long command line test (got $OUT words)"
else
    pass_test "long command line handling"
fi

ALIAS_SETUP=""
for i in $(seq 1 50); do
    ALIAS_SETUP="$ALIAS_SETUP alias test$i='echo alias$i';"
done
OUT=$("$CJSH_PATH" -c "$ALIAS_SETUP test25" 2>/dev/null)
if [ "$OUT" != "alias25" ]; then
    fail_test "many aliases test (got '$OUT')"
else
    pass_test "many aliases handling"
fi

start_time=$(date +%s)
for i in $(seq 1 5); do
    "$CJSH_PATH" -c "true" >/dev/null 2>&1
done
end_time=$(date +%s)
duration=$((end_time - start_time))

if [ "$duration" -gt 10 ]; then
    fail_test "5 shell invocations took more than 10 seconds"
else
    pass_test "rapid command execution"
fi

MEMORY_TEST=""
for i in $(seq 1 100); do
    MEMORY_TEST="$MEMORY_TEST VAR$i='$(seq 1 100 | tr '\n' ' ')';"
done
"$CJSH_PATH" -c "$MEMORY_TEST echo done" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    fail_test "memory stress test failed"
else
    pass_test "memory stress handling"
fi

{
    "$CJSH_PATH" -c "sleep 0.1; echo concurrent1" &
    "$CJSH_PATH" -c "sleep 0.1; echo concurrent2" &
    "$CJSH_PATH" -c "sleep 0.1; echo concurrent3" &
    wait
} >/dev/null 2>&1

if [ $? -ne 0 ]; then
    fail_test "concurrent execution test had issues"
else
    pass_test "concurrent execution handling"
fi

FD_TEST=""
for i in $(seq 1 10); do
    FD_TEST="$FD_TEST echo test$i > /tmp/cjsh_test_$i.txt;"
done
FD_TEST="$FD_TEST rm -f /tmp/cjsh_test_*.txt"
"$CJSH_PATH" -c "$FD_TEST" 2>/dev/null
if [ $? -ne 0 ]; then
    fail_test "file descriptor test failed"
else
    pass_test "file descriptor handling"
fi

echo "PASS"
exit 0
