#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: expansion performance and stress..."

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

OUT=$("$CJSH_PATH" -c "echo {1..100}" 2>&1)
if echo "$OUT" | wc -w | grep -q "100"; then
    pass_test "large integer range {1..100}"
else
    fail_test "large integer range {1..100} (got word count: $(echo "$OUT" | wc -w))"
fi

OUT=$("$CJSH_PATH" -c "echo {a..z}" 2>&1)
if echo "$OUT" | wc -w | grep -q "26"; then
    pass_test "full alphabet range {a..z}"
else
    fail_test "full alphabet range {a..z} (got word count: $(echo "$OUT" | wc -w))"
fi

OUT=$("$CJSH_PATH" -c "echo {1..50000}" 2>&1)
WORD_COUNT=$(echo "$OUT" | wc -w)
if [ "$WORD_COUNT" -eq 1 ] && [ "$OUT" = "{1..50000}" ]; then
    pass_test "very large range bounded (returned literal)"
elif [ "$WORD_COUNT" -eq 50000 ]; then
    pass_test "very large range expanded fully"
else
    fail_test "very large range {1..50000} (got word count: $WORD_COUNT)"
fi

OUT=$("$CJSH_PATH" -c "echo {a,b}{1,2}{x,y}" 2>&1)
EXPECTED_COUNT=8  # 2 * 2 * 2 = 8 combinations
ACTUAL_COUNT=$(echo "$OUT" | wc -w)
if [ "$ACTUAL_COUNT" -eq "$EXPECTED_COUNT" ]; then
    pass_test "triple nested braces {a,b}{1,2}{x,y}"
else
    fail_test "triple nested braces {a,b}{1,2}{x,y} (got: $ACTUAL_COUNT, expected: $EXPECTED_COUNT)"
fi

OUT=$("$CJSH_PATH" -c "echo {1..3}{a..c}" 2>&1)
EXPECTED_COUNT=9  # 3 * 3 = 9 combinations
ACTUAL_COUNT=$(echo "$OUT" | wc -w)
if [ "$ACTUAL_COUNT" -eq "$EXPECTED_COUNT" ]; then
    pass_test "nested range expansion {1..3}{a..c}"
else
    fail_test "nested range expansion {1..3}{a..c} (got: $ACTUAL_COUNT, expected: $EXPECTED_COUNT)"
fi

OUT=$("$CJSH_PATH" -c "echo {a,{b,c},d}" 2>&1)
if echo "$OUT" | grep -q "a" && echo "$OUT" | grep -q "b" && echo "$OUT" | grep -q "c" && echo "$OUT" | grep -q "d"; then
    pass_test "complex nested comma expansion {a,{b,c},d}"
else
    fail_test "complex nested comma expansion {a,{b,c},d} (got: '$OUT')"
fi

START_TIME=$(date +%s)
"$CJSH_PATH" -c "echo {1..1000}" >/dev/null 2>&1
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
if [ "$DURATION" -le 5 ]; then
    pass_test "expansion performance {1..1000} completed in ${DURATION}s"
else
    fail_test "expansion performance {1..1000} too slow (${DURATION}s)"
fi

OUT=$("$CJSH_PATH" -c "echo {1..500}" 2>&1)
WORD_COUNT=$(echo "$OUT" | wc -w)
if [ "$WORD_COUNT" -eq 500 ]; then
    pass_test "memory stress test {1..500}"
else
    fail_test "memory stress test {1..500} (got word count: $WORD_COUNT)"
fi

OUT=$("$CJSH_PATH" -c "echo {a,b}{1..3}" 2>&1)
EXPECTED_COUNT=6  # 2 * 3 = 6 combinations
ACTUAL_COUNT=$(echo "$OUT" | wc -w)
if [ "$ACTUAL_COUNT" -eq "$EXPECTED_COUNT" ]; then
    pass_test "mixed expansion types {a,b}{1..3}"
else
    fail_test "mixed expansion types {a,b}{1..3} (got: $ACTUAL_COUNT, expected: $EXPECTED_COUNT)"
fi

OUT=$("$CJSH_PATH" -c "echo very_long_prefix_{1..3}_very_long_suffix" 2>&1)
if echo "$OUT" | grep -q "very_long_prefix_1_very_long_suffix"; then
    pass_test "expansion with long prefix/suffix"
else
    fail_test "expansion with long prefix/suffix (got: '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "echo {a,b,c,d,e,f,g,h,i,j}" 2>&1)
EXPECTED_COUNT=10
ACTUAL_COUNT=$(echo "$OUT" | wc -w)
if [ "$ACTUAL_COUNT" -eq "$EXPECTED_COUNT" ]; then
    pass_test "many comma-separated options"
else
    fail_test "many comma-separated options (got: $ACTUAL_COUNT, expected: $EXPECTED_COUNT)"
fi

OUT=$("$CJSH_PATH" -c "echo {{{{a,b}}}}" 2>&1)
if echo "$OUT" | grep -q "a" && echo "$OUT" | grep -q "b"; then
    pass_test "pathological nesting {{{{a,b}}}}"
else
    if [ "$OUT" = "{{{{a,b}}}}" ]; then
        pass_test "pathological nesting {{{{a,b}}}} (returned literal)"
    else
        fail_test "pathological nesting {{{{a,b}}}} (got: '$OUT')"
    fi
fi

OUT=$("$CJSH_PATH" -c "echo {1..100000}" 2>&1)
if [ "$OUT" = "{1..100000}" ]; then
    pass_test "huge range protection (returned literal)"
elif echo "$OUT" | wc -w | grep -q "100000"; then
    pass_test "huge range expansion (allowed)"
else
    fail_test "huge range {1..100000} (got: '$OUT')"
fi

START_TIME=$(date +%s)
for i in 1 2 3 4 5; do
    "$CJSH_PATH" -c "echo {a..z}" >/dev/null 2>&1
done
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
if [ "$DURATION" -le 3 ]; then
    pass_test "rapid successive expansions completed in ${DURATION}s"
else
    fail_test "rapid successive expansions too slow (${DURATION}s)"
fi

OUT=$("$CJSH_PATH" -c "echo {5..5}" 2>&1)
if [ "$OUT" = "5" ]; then
    pass_test "single element range {5..5}"
else
    fail_test "single element range {5..5} (got: '$OUT')"
fi

echo ""
echo "Expansion Performance Tests Summary:"
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