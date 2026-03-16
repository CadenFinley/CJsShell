#!/usr/bin/env sh
# test_zombie_edge_cases.sh
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: advanced zombie process scenarios..."

count_zombies() {
    if command -v ps >/dev/null 2>&1; then
        ps axo stat 2>/dev/null | awk '$1 ~ /^Z/ { count++ } END { print count+0 }'
    else
        echo 0
    fi
}

wait_for_zombie_cleanup() {
    local baseline="$1"
    local max_checks="${2:-20}"
    local delay="${3:-0.1}"
    local count=0
    local current="$baseline"
    while [ "$count" -lt "$max_checks" ]; do
        current=$(count_zombies)
        if [ "$current" -le "$baseline" ]; then
            echo "$current"
            return 0
        fi
        sleep "$delay"
        count=$((count + 1))
    done
    echo "$current"
    return 1
}

BASELINE_ZOMBIES=$(count_zombies)

"$CJSH_PATH" -c 'true & true & true & true & true & wait'
if [ $? -eq 0 ]; then
    echo "PASS: rapid forking"
else
    echo "FAIL: rapid forking caused issues"
    exit 1
fi

"$CJSH_PATH" -c '(sleep 0.1 & sleep 0.1 & wait) & wait'
if [ $? -eq 0 ]; then
    echo "PASS: nested background processes"
else
    echo "FAIL: nested background processes"
    exit 1
fi

"$CJSH_PATH" -c 'true & true & true & wait'
if [ $? -eq 0 ]; then
    echo "PASS: background process with children"
else
    echo "FAIL: background process with children"
    exit 1
fi

"$CJSH_PATH" -c 'echo test | cat > /dev/null'
if [ $? -eq 0 ]; then
    echo "PASS: pipeline with background components"
else
    echo "FAIL: pipeline with background components"
    exit 1
fi

OUT=$("$CJSH_PATH" -c 'echo substituted')
if [ "$OUT" = "substituted" ]; then
    echo "PASS: command substitution in background"
else
    echo "FAIL: command substitution in background (got '$OUT')"
    exit 1
fi

"$CJSH_PATH" -c '(echo deep > /dev/null) & wait'
if [ $? -eq 0 ]; then
    echo "PASS: multiple levels of nesting"
else
    echo "FAIL: multiple levels of nesting"
    exit 1
fi

"$CJSH_PATH" -c 'true & false & sleep 0.1 & wait'
if [ $? -eq 0 ]; then
    echo "PASS: mixed exit codes in background"
else
    echo "FAIL: mixed exit codes in background"
    exit 1
fi

"$CJSH_PATH" -c 'echo test | cat | wc -l > /dev/null'
if [ $? -eq 0 ]; then
    echo "PASS: process substitution with background"
else
    echo "FAIL: process substitution with background"
    exit 1
fi

"$CJSH_PATH" -c 'echo background > /tmp/cjsh_zombie_test_bg.txt & wait; cat /tmp/cjsh_zombie_test_bg.txt > /dev/null; rm -f /tmp/cjsh_zombie_test_bg.txt'
if [ $? -eq 0 ]; then
    echo "PASS: background job with I/O redirection"
else
    echo "FAIL: background job with I/O redirection"
    exit 1
fi

"$CJSH_PATH" -c 'true & true & true & true & true & true & true & true & true & true & wait'
if [ $? -eq 0 ]; then
    echo "PASS: stress test with many processes"
else
    echo "FAIL: stress test with many processes"
    exit 1
fi

"$CJSH_PATH" -c 'sleep 2 & PID=$!; kill -TERM $PID; wait $PID 2>/dev/null'
if [ $? -ne 0 ]; then  # Should fail because process was terminated
    echo "PASS: background process with signal handling"
else
    echo "FAIL: background process with signal handling"
    exit 1
fi

"$CJSH_PATH" -c 'true & sleep 0.05; wait'
if [ $? -eq 0 ]; then
    echo "PASS: quick-exiting background job"
else
    echo "FAIL: quick-exiting background job"
    exit 1
fi

"$CJSH_PATH" -c 'echo line1; echo line2; echo line3 | grep line | wc -l > /dev/null'
if [ $? -eq 0 ]; then
    echo "PASS: complex pipeline with background elements"
else
    echo "FAIL: complex pipeline with background elements"
    exit 1
fi

export ZOMBIE_TEST_VAR="test_value"
"$CJSH_PATH" -c 'echo $ZOMBIE_TEST_VAR > /dev/null & wait'
if [ $? -eq 0 ]; then
    echo "PASS: background process with environment variables"
else
    echo "FAIL: background process with environment variables"
    exit 1
fi

if ZOMBIE_COUNT=$(wait_for_zombie_cleanup "$BASELINE_ZOMBIES" 30 0.1); then
    echo "PASS: no zombie accumulation after tests"
else
    ZOMBIE_INCREASE=$((ZOMBIE_COUNT - BASELINE_ZOMBIES))
    echo "FAIL: found $ZOMBIE_INCREASE new zombie processes after tests (baseline: $BASELINE_ZOMBIES, current: $ZOMBIE_COUNT)"
    echo "All zombie processes found:"
    ps axo stat,pid,ppid,command 2>/dev/null | awk '$1 ~ /^Z/ { print }'
    exit 1
fi

echo "PASS: All advanced zombie process tests completed successfully"
exit 0
