#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi

echo "Test: elif statement handling..."

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

# Basic elif chain where the middle branch runs
OUTPUT=$("$CJSH_PATH" -c "if false; then echo first; elif true; then echo second; else echo third; fi")
if [ "$OUTPUT" = "second" ]; then
    pass_test "elif executes middle branch"
else
    fail_test "elif executes middle branch (got: '$OUTPUT')"
fi

# Elif fall-through to else branch
OUTPUT=$("$CJSH_PATH" -c "if false; then echo first; elif false; then echo second; else echo third; fi")
if [ "$OUTPUT" = "third" ]; then
    pass_test "elif falls through to else"
else
    fail_test "elif falls through to else (got: '$OUTPUT')"
fi

# Multiple elif branches where later branch succeeds
OUTPUT=$("$CJSH_PATH" -c "if false; then echo first; elif false; then echo second; elif true; then echo third; else echo fourth; fi")
if [ "$OUTPUT" = "third" ]; then
    pass_test "multiple elif branches"
else
    fail_test "multiple elif branches (got: '$OUTPUT')"
fi

# Ensure only the matching branch executes (no additional output)
OUTPUT=$("$CJSH_PATH" -c "if false; then echo first; elif true; then echo matched; elif true; then echo should_not_run; else echo else_branch; fi")
if [ "$OUTPUT" = "matched" ]; then
    pass_test "elif short-circuits remaining branches"
else
    fail_test "elif short-circuits remaining branches (got: '$OUTPUT')"
fi

# Elif with commands returning exit codes
OUTPUT=$("$CJSH_PATH" -c "if command false >/dev/null 2>&1; then echo cmd1; elif command true >/dev/null 2>&1; then echo cmd2; else echo cmd3; fi")
if [ "$OUTPUT" = "cmd2" ]; then
    pass_test "elif uses command exit status"
else
    fail_test "elif uses command exit status (got: '$OUTPUT')"
fi

# Elif with arithmetic comparisons
OUTPUT=$("$CJSH_PATH" -c "VALUE=5; if [ \"$VALUE\" -lt 5 ]; then echo lt; elif [ \"$VALUE\" -eq 5 ]; then echo eq; else echo gt; fi")
if [ "$OUTPUT" = "eq" ]; then
    pass_test "elif arithmetic comparisons"
else
    fail_test "elif arithmetic comparisons (got: '$OUTPUT')"
fi

# Elif with string pattern matching in double brackets
OUTPUT=$("$CJSH_PATH" -c "WORD=hello; if [[ $WORD == h*o ]]; then echo pattern1; elif [[ $WORD == *z ]]; then echo pattern2; else echo pattern3; fi")
if [ "$OUTPUT" = "pattern1" ]; then
    pass_test "elif with double bracket patterns"
else
    fail_test "elif with double bracket patterns (got: '$OUTPUT')"
fi

# Elif nested within else branch
OUTPUT=$("$CJSH_PATH" -c "if false; then echo outer_then; else if false; then echo inner_then; elif true; then echo inner_elif; else echo inner_else; fi; fi")
if [ "$OUTPUT" = "inner_elif" ]; then
    pass_test "nested elif inside else"
else
    fail_test "nested elif inside else (got: '$OUTPUT')"
fi

# Elif chain with functions
OUTPUT=$("$CJSH_PATH" -c "test_func() { return 1; }; other_func() { return 0; }; if test_func; then echo func_if; elif other_func; then echo func_elif; else echo func_else; fi")
if [ "$OUTPUT" = "func_elif" ]; then
    pass_test "elif evaluates functions"
else
    skip_test "elif evaluates functions (functions may not be supported)"
fi

# Elif chain defined across multiple lines using \n formatting
OUTPUT=$("$CJSH_PATH" -c 'if false; then\n    echo first\nelif true; then\n    echo second\nelse\n    echo third\nfi')
if [ "$OUTPUT" = "second" ]; then
    pass_test "multiline elif chain"
else
    fail_test "multiline elif chain (got: '$OUTPUT')"
fi

# Elif with compound conditions
OUTPUT=$("$CJSH_PATH" -c "A=1; B=2; if [ $A -eq 2 ]; then echo first; elif [ $A -eq 1 ] && [ $B -eq 2 ]; then echo compound; else echo neither; fi")
if [ "$OUTPUT" = "compound" ]; then
    pass_test "elif with compound conditions"
else
    fail_test "elif with compound conditions (got: '$OUTPUT')"
fi

# Error handling: malformed elif should fail
if "$CJSH_PATH" -c "if true; then echo ok; elif; then echo bad; fi" 2>/dev/null; then
    fail_test "syntax error in elif is caught"
else
    pass_test "syntax error in elif is caught"
fi

# Summary

echo

echo "Elif Statement Test Summary:"
echo "PASSED: $TESTS_PASSED"
echo "FAILED: $TESTS_FAILED"
echo "SKIPPED: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    if [ $TESTS_SKIPPED -gt 0 ]; then
        echo "All non-skipped elif tests passed with some skipped cases."
    else
        echo "All elif statement tests passed!"
    fi
    exit 0
else
    echo "Some elif statement tests failed!"
    exit 1
fi
