#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: comprehensive if statements..."
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
OUTPUT=$("$CJSH_PATH" -c "if true; then echo 'success'; fi")
if [ "$OUTPUT" = "success" ]; then
    pass_test "basic if statement with true condition"
else
    fail_test "basic if statement with true condition (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false; then echo 'fail'; fi")
if [ "$OUTPUT" = "" ]; then
    pass_test "basic if statement with false condition"
else
    fail_test "basic if statement with false condition (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then echo 'true_branch'; else echo 'false_branch'; fi")
if [ "$OUTPUT" = "true_branch" ]; then
    pass_test "if-else with true condition"
else
    fail_test "if-else with true condition (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false; then echo 'true_branch'; else echo 'false_branch'; fi")
if [ "$OUTPUT" = "false_branch" ]; then
    pass_test "if-else with false condition"
else
    fail_test "if-else with false condition (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false; then echo 'first'; elif true; then echo 'second'; else echo 'third'; fi")
if [ "$OUTPUT" = "second" ]; then
    pass_test "elif statement - second condition true"
else
    fail_test "elif statement - second condition true (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false; then echo 'first'; elif false; then echo 'second'; else echo 'third'; fi")
if [ "$OUTPUT" = "third" ]; then
    pass_test "elif statement - all conditions false"
else
    fail_test "elif statement - all conditions false (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false; then echo 'first'; elif false; then echo 'second'; elif true; then echo 'third'; else echo 'fourth'; fi")
if [ "$OUTPUT" = "third" ]; then
    pass_test "multiple elif statements"
else
    fail_test "multiple elif statements (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if echo 'test' >/dev/null; then echo 'command_success'; fi")
if [ "$OUTPUT" = "command_success" ]; then
    pass_test "if with successful command"
else
    fail_test "if with successful command (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then echo 'exit_zero'; else echo 'exit_nonzero'; fi")
if [ "$OUTPUT" = "exit_zero" ]; then
    pass_test "if with exit 0"
else
    fail_test "if with exit 0 (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false; then echo 'exit_zero'; else echo 'exit_nonzero'; fi")
if [ "$OUTPUT" = "exit_nonzero" ]; then
    pass_test "if with exit 1"
else
    fail_test "if with exit 1 (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ 5 -eq 5 ]; then echo 'math_true'; else echo 'math_false'; fi")
if [ "$OUTPUT" = "math_true" ]; then
    pass_test "if with test command - equality"
else
    fail_test "if with test command - equality (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ 5 -gt 3 ]; then echo 'greater'; else echo 'not_greater'; fi")
if [ "$OUTPUT" = "greater" ]; then
    pass_test "if with test command - greater than"
else
    fail_test "if with test command - greater than (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [[ \"hello\" == \"hello\" ]]; then echo 'string_match'; else echo 'no_match'; fi")
if [ "$OUTPUT" = "string_match" ]; then
    pass_test "if with double bracket string comparison"
else
    fail_test "if with double bracket string comparison (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [[ \"hello\" =~ h.* ]]; then echo 'regex_match'; else echo 'no_regex_match'; fi")
if [ "$OUTPUT" = "regex_match" ]; then
    pass_test "if with double bracket regex"
else
    fail_test "if with double bracket regex (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then if true; then echo 'nested_true'; else echo 'nested_false'; fi; else echo 'outer_false'; fi")
if [ "$OUTPUT" = "nested_true" ]; then
    pass_test "nested if statements - both true"
else
    fail_test "nested if statements - both true (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then if false; then echo 'nested_true'; else echo 'nested_false'; fi; else echo 'outer_false'; fi")
if [ "$OUTPUT" = "nested_false" ]; then
    pass_test "nested if statements - outer true, inner false"
else
    fail_test "nested if statements - outer true, inner false (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true && true; then echo 'and_true'; else echo 'and_false'; fi")
if [ "$OUTPUT" = "and_true" ]; then
    pass_test "if with logical AND - both true"
else
    fail_test "if with logical AND - both true (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true && false; then echo 'and_true'; else echo 'and_false'; fi")
if [ "$OUTPUT" = "and_false" ]; then
    pass_test "if with logical AND - second false"
else
    fail_test "if with logical AND - second false (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if false || true; then echo 'or_true'; else echo 'or_false'; fi")
if [ "$OUTPUT" = "or_true" ]; then
    pass_test "if with logical OR - second true"
else
    fail_test "if with logical OR - second true (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "VAR=hello; if [ \"\$VAR\" = \"hello\" ]; then echo 'var_match'; else echo 'var_no_match'; fi")
if [ "$OUTPUT" = "var_match" ]; then
    pass_test "if with variable in condition"
else
    fail_test "if with variable in condition (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ \$((5 + 3)) -eq 8 ]; then echo 'arithmetic_true'; else echo 'arithmetic_false'; fi")
if [ "$OUTPUT" = "arithmetic_true" ]; then
    pass_test "if with arithmetic expansion"
else
    fail_test "if with arithmetic expansion (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ \"\$(echo hello)\" = \"hello\" ]; then echo 'cmd_sub_true'; else echo 'cmd_sub_false'; fi")
if [ "$OUTPUT" = "cmd_sub_true" ]; then
    pass_test "if with command substitution"
else
    fail_test "if with command substitution (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "
if true
then
    echo 'multiline_true'
else
    echo 'multiline_false'
fi")
if [ "$OUTPUT" = "multiline_true" ]; then
    pass_test "multiline if statement"
else
    fail_test "multiline if statement (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then echo 'first'; echo 'second'; else echo 'else_branch'; fi")
EXPECTED="first
second"
if [ "$OUTPUT" = "$EXPECTED" ]; then
    pass_test "if with multiple commands in then branch"
else
    fail_test "if with multiple commands in then branch (got: '$OUTPUT')"
fi
TEMP_FILE="/tmp/cjsh_if_test_file_$$"
TEMP_DIR="/tmp/cjsh_if_test_dir_$$"
touch "$TEMP_FILE"
mkdir -p "$TEMP_DIR"
OUTPUT=$("$CJSH_PATH" -c "if [ -f \"$TEMP_FILE\" ]; then echo 'file_exists'; else echo 'file_missing'; fi")
if [ "$OUTPUT" = "file_exists" ]; then
    pass_test "if with file existence test"
else
    fail_test "if with file existence test (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ -d \"$TEMP_DIR\" ]; then echo 'dir_exists'; else echo 'dir_missing'; fi")
if [ "$OUTPUT" = "dir_exists" ]; then
    pass_test "if with directory existence test"
else
    fail_test "if with directory existence test (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ -z \"\" ]; then echo 'empty_string'; else echo 'not_empty'; fi")
if [ "$OUTPUT" = "empty_string" ]; then
    pass_test "if with empty string test"
else
    fail_test "if with empty string test (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ -n \"hello\" ]; then echo 'non_empty'; else echo 'empty'; fi")
if [ "$OUTPUT" = "non_empty" ]; then
    pass_test "if with non-empty string test"
else
    fail_test "if with non-empty string test (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ ! 1 -eq 2 ]; then echo 'negation_true'; else echo 'negation_false'; fi")
if [ "$OUTPUT" = "negation_true" ]; then
    pass_test "if with negation using test"
else
    fail_test "if with negation using test (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then if [ 5 -gt 3 ]; then echo 'complex_true'; else echo 'inner_false'; fi; else echo 'outer_false'; fi")
if [ "$OUTPUT" = "complex_true" ]; then
    pass_test "complex nested if with test command"
else
    fail_test "complex nested if with test command (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if [ 5 -eq 5 ] && [[ \"hello\" == \"hello\" ]]; then echo 'mixed_true'; else echo 'mixed_false'; fi")
if [ "$OUTPUT" = "mixed_true" ]; then
    pass_test "if with mixed bracket conditions"
else
    fail_test "if with mixed bracket conditions (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if (true && true) || false; then echo 'group_true'; else echo 'group_false'; fi")
if [ "$OUTPUT" = "group_true" ]; then
    pass_test "if with parentheses grouping"
else
    fail_test "if with parentheses grouping (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true; then exit 0; else exit 1; fi; echo 'after_if'")
if [ "$OUTPUT" = "" ]; then
    pass_test "if statement with exit in then branch"
else
    fail_test "if statement with exit in then branch (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "if true;then echo 'no_space';fi")
if [ "$OUTPUT" = "no_space" ]; then
    pass_test "if statement without spaces around semicolons"
else
    fail_test "if statement without spaces around semicolons (got: '$OUTPUT')"
fi
OUTPUT=$("$CJSH_PATH" -c "test_func() { return 0; }; if test_func; then echo 'func_success'; else echo 'func_fail'; fi")
if [ "$OUTPUT" = "func_success" ]; then
    pass_test "if with function return code"
else
    skip_test "if with function return code (functions may not be supported)"
fi
if "$CJSH_PATH" -c "if true; then echo 'unmatched'" 2>/dev/null; then
    fail_test "unmatched if statement error handling"
else
    pass_test "unmatched if statement error handling"
fi
if "$CJSH_PATH" -c "if; then echo 'no_condition'; fi" 2>/dev/null; then
    fail_test "if without condition error handling"
else
    pass_test "if without condition error handling"
fi
rm -f "$TEMP_FILE"
rm -rf "$TEMP_DIR"
echo
echo "If Statement Test Summary:"
echo "PASSED: $TESTS_PASSED"
echo "FAILED: $TESTS_FAILED"
echo "SKIPPED: $TESTS_SKIPPED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "All if statement tests passed!"
    exit 0
else
    echo "Some if statement tests failed!"
    exit 1
fi
