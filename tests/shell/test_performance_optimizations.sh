#!/usr/bin/env sh
# Test performance optimizations and caching
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: performance optimizations..."

# Create temporary test directory
TEST_DIR="/tmp/cjsh_performance_tests_$$"
mkdir -p "$TEST_DIR"

# Test 1: Script execution time (baseline)
START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" -c 'for i in 1 2 3 4 5; do echo "iteration $i"; done' 2>&1)
END_TIME=$(date +%s%N)
EXECUTION_TIME=$((END_TIME - START_TIME))

# Check if we got all 5 iterations
if echo "$OUT" | grep -q "iteration 1" && echo "$OUT" | grep -q "iteration 5"; then
    echo "PASS: baseline script execution (time: ${EXECUTION_TIME}ns)"
else
    echo "FAIL: baseline script execution failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 2: Repeated function calls (test caching potential)
cat > "$TEST_DIR/repeated_functions.sh" << 'EOF'
#!/bin/bash
test_function() {
    echo "function called with: $1"
}

for i in 1 2 3 4 5; do
    test_function $i
done
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" "$TEST_DIR/repeated_functions.sh" 2>&1)
END_TIME=$(date +%s%N)
FUNCTION_TIME=$((END_TIME - START_TIME))

if echo "$OUT" | grep -q "function called with: 1"; then
    echo "PASS: repeated function calls (time: ${FUNCTION_TIME}ns)"
else
    echo "FAIL: repeated function calls failed"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 3: Complex variable expansion performance
cat > "$TEST_DIR/variable_expansion.sh" << 'EOF'
#!/bin/bash
var="hello world test string"
for i in 1 2 3 4 5 6 7 8 9 10; do
    result=${var#hello }
    result=${result% string}
    echo $result
done
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" "$TEST_DIR/variable_expansion.sh" 2>&1)
END_TIME=$(date +%s%N)
EXPANSION_TIME=$((END_TIME - START_TIME))

if echo "$OUT" | grep -q "world test"; then
    echo "PASS: variable expansion performance (time: ${EXPANSION_TIME}ns)"
else
    echo "FAIL: variable expansion performance test failed"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 4: Command substitution performance
cat > "$TEST_DIR/command_substitution.sh" << 'EOF'
#!/bin/bash
for i in 1 2 3; do
    result=$(echo "test $i")
    echo "Result: $result"
done
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" "$TEST_DIR/command_substitution.sh" 2>&1)
END_TIME=$(date +%s%N)
SUBSTITUTION_TIME=$((END_TIME - START_TIME))

if echo "$OUT" | grep -q "Result: test 1"; then
    echo "PASS: command substitution performance (time: ${SUBSTITUTION_TIME}ns)"
else
    echo "FAIL: command substitution performance test failed"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 5: Large loop performance
cat > "$TEST_DIR/large_loop.sh" << 'EOF'
#!/bin/bash
count=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    count=$((count + 1))
done
echo "Final count: $count"
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" "$TEST_DIR/large_loop.sh" 2>&1)
END_TIME=$(date +%s%N)
LOOP_TIME=$((END_TIME - START_TIME))

if [ "$OUT" = "Final count: 20" ]; then
    echo "PASS: large loop performance (time: ${LOOP_TIME}ns)"
else
    echo "FAIL: large loop performance test failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 6: Nested control structure performance
cat > "$TEST_DIR/nested_structures.sh" << 'EOF'
#!/bin/bash
result=0
for i in 1 2 3; do
    for j in 1 2 3; do
        if [ $i -eq $j ]; then
            result=$((result + 1))
        fi
    done
done
echo "Result: $result"
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" "$TEST_DIR/nested_structures.sh" 2>&1)
END_TIME=$(date +%s%N)
NESTED_TIME=$((END_TIME - START_TIME))

if [ "$OUT" = "Result: 3" ]; then
    echo "PASS: nested structure performance (time: ${NESTED_TIME}ns)"
else
    echo "FAIL: nested structure performance test failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 7: Memory usage test (approximate)
cat > "$TEST_DIR/memory_test.sh" << 'EOF'
#!/bin/bash
# Create many variables to test memory usage
for i in 1 2 3 4 5 6 7 8 9 10; do
    eval "var_$i='This is variable number $i with some text'"
done
echo "Variables created"
EOF

OUT=$("$CJSH_PATH" "$TEST_DIR/memory_test.sh" 2>&1)
if [ "$OUT" = "Variables created" ]; then
    echo "PASS: memory usage test completed"
else
    echo "FAIL: memory usage test failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 8: File I/O performance
echo "line1" > "$TEST_DIR/input.txt"
echo "line2" >> "$TEST_DIR/input.txt"
echo "line3" >> "$TEST_DIR/input.txt"

cat > "$TEST_DIR/file_io_test.sh" << 'EOF'
#!/bin/bash
while read line; do
    echo "Read: $line"
done < input.txt
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" -c "cd $TEST_DIR && $CJSH_PATH file_io_test.sh" 2>&1)
END_TIME=$(date +%s%N)
IO_TIME=$((END_TIME - START_TIME))

if echo "$OUT" | grep -q "Read: line1"; then
    echo "PASS: file I/O performance (time: ${IO_TIME}ns)"
else
    echo "FAIL: file I/O performance test failed"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 9: Arithmetic performance
cat > "$TEST_DIR/arithmetic_test.sh" << 'EOF'
#!/bin/bash
result=0
for i in 1 2 3 4 5 6 7 8 9 10; do
    result=$((result + i * 2))
done
echo "Arithmetic result: $result"
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" "$TEST_DIR/arithmetic_test.sh" 2>&1)
END_TIME=$(date +%s%N)
ARITHMETIC_TIME=$((END_TIME - START_TIME))

if [ "$OUT" = "Arithmetic result: 110" ]; then
    echo "PASS: arithmetic performance (time: ${ARITHMETIC_TIME}ns)"
else
    echo "FAIL: arithmetic performance test failed (got: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 10: Script parsing performance (syntax checking)
cat > "$TEST_DIR/complex_syntax.sh" << 'EOF'
#!/bin/bash
function test_func() {
    if [ "$1" = "test" ]; then
        for i in 1 2 3; do
            case $i in
                1) echo "one";;
                2) echo "two";;
                *) echo "other";;
            esac
        done
    fi
}
test_func "test"
EOF

START_TIME=$(date +%s%N)
OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/complex_syntax.sh" 2>&1)
END_TIME=$(date +%s%N)
PARSE_TIME=$((END_TIME - START_TIME))

if echo "$OUT" | grep -q "No syntax errors found"; then
    echo "PASS: syntax parsing performance (time: ${PARSE_TIME}ns)"
else
    echo "FAIL: syntax parsing performance test failed"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Performance summary
echo "Performance Summary:"
echo "  Baseline execution: ${EXECUTION_TIME}ns"
echo "  Function calls: ${FUNCTION_TIME}ns"
echo "  Variable expansion: ${EXPANSION_TIME}ns"
echo "  Command substitution: ${SUBSTITUTION_TIME}ns"
echo "  Large loops: ${LOOP_TIME}ns"
echo "  Nested structures: ${NESTED_TIME}ns"
echo "  File I/O: ${IO_TIME}ns"
echo "  Arithmetic: ${ARITHMETIC_TIME}ns"
echo "  Syntax parsing: ${PARSE_TIME}ns"

# Cleanup
rm -rf "$TEST_DIR"
echo "PASS: performance optimization tests completed"
exit 0