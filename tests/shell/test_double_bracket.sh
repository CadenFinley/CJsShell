#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: double bracket expressions..."

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

# Test string length checks
if "$CJSH_PATH" -c '[[ -z "" ]]'; then
    pass_test "double bracket -z empty string"
else
    fail_test "double bracket -z empty string"
fi

if "$CJSH_PATH" -c '[[ -n "hello" ]]'; then
    pass_test "double bracket -n non-empty string"
else
    fail_test "double bracket -n non-empty string"
fi

if "$CJSH_PATH" -c '[[ ! -n "" ]]'; then
    pass_test "double bracket negation -n empty string"
else
    fail_test "double bracket negation -n empty string"
fi

# Test string equality
if "$CJSH_PATH" -c '[[ "hello" == "hello" ]]'; then
    pass_test "double bracket string equality =="
else
    fail_test "double bracket string equality =="
fi

if "$CJSH_PATH" -c '[[ "hello" = "hello" ]]'; then
    pass_test "double bracket string equality ="
else
    fail_test "double bracket string equality ="
fi

if "$CJSH_PATH" -c '[[ "hello" != "world" ]]'; then
    pass_test "double bracket string inequality"
else
    fail_test "double bracket string inequality"
fi

# Test pattern matching
if "$CJSH_PATH" -c '[[ "hello" == h* ]]'; then
    pass_test "double bracket pattern matching with *"
else
    fail_test "double bracket pattern matching with *"
fi

if "$CJSH_PATH" -c '[[ "hello" == h?llo ]]'; then
    pass_test "double bracket pattern matching with ?"
else
    fail_test "double bracket pattern matching with ?"
fi

if "$CJSH_PATH" -c '[[ "hello" == h[aeiou]llo ]]'; then
    pass_test "double bracket pattern matching with character class"
else
    fail_test "double bracket pattern matching with character class"
fi

# Test regex matching
if "$CJSH_PATH" -c '[[ "hello123" =~ [0-9]+ ]]'; then
    pass_test "double bracket regex matching"
else
    fail_test "double bracket regex matching"
fi

if "$CJSH_PATH" -c '[[ "hello" =~ ^h.*o$ ]]'; then
    pass_test "double bracket regex anchors"
else
    fail_test "double bracket regex anchors"
fi

# Test numeric comparisons
if "$CJSH_PATH" -c '[[ 5 -eq 5 ]]'; then
    pass_test "double bracket numeric equality"
else
    fail_test "double bracket numeric equality"
fi

if "$CJSH_PATH" -c '[[ 5 -ne 3 ]]'; then
    pass_test "double bracket numeric inequality"
else
    fail_test "double bracket numeric inequality"
fi

if "$CJSH_PATH" -c '[[ 5 -gt 3 ]]'; then
    pass_test "double bracket greater than"
else
    fail_test "double bracket greater than"
fi

if "$CJSH_PATH" -c '[[ 3 -lt 5 ]]'; then
    pass_test "double bracket less than"
else
    fail_test "double bracket less than"
fi

if "$CJSH_PATH" -c '[[ 5 -ge 5 ]]'; then
    pass_test "double bracket greater than or equal"
else
    fail_test "double bracket greater than or equal"
fi

if "$CJSH_PATH" -c '[[ 3 -le 5 ]]'; then
    pass_test "double bracket less than or equal"
else
    fail_test "double bracket less than or equal"
fi

# Test file operations (create temporary files for testing)
TEMP_FILE="/tmp/cjsh_test_file_$$"
TEMP_DIR="/tmp/cjsh_test_dir_$$"
touch "$TEMP_FILE"
mkdir -p "$TEMP_DIR"
echo "test content" > "$TEMP_FILE"

if "$CJSH_PATH" -c "[[ -f \"$TEMP_FILE\" ]]"; then
    pass_test "double bracket file existence -f"
else
    fail_test "double bracket file existence -f"
fi

if "$CJSH_PATH" -c "[[ -d \"$TEMP_DIR\" ]]"; then
    pass_test "double bracket directory existence -d"
else
    fail_test "double bracket directory existence -d"
fi

if "$CJSH_PATH" -c "[[ -e \"$TEMP_FILE\" ]]"; then
    pass_test "double bracket path existence -e"
else
    fail_test "double bracket path existence -e"
fi

if "$CJSH_PATH" -c "[[ -r \"$TEMP_FILE\" ]]"; then
    pass_test "double bracket readable -r"
else
    fail_test "double bracket readable -r"
fi

if "$CJSH_PATH" -c "[[ -s \"$TEMP_FILE\" ]]"; then
    pass_test "double bracket non-empty file -s"
else
    fail_test "double bracket non-empty file -s"
fi

# Test logical operators
if "$CJSH_PATH" -c '[[ "hello" == "hello" && "world" == "world" ]]' 2>/dev/null; then
    pass_test "double bracket logical AND (&&) - both true"
else
    fail_test "double bracket logical AND (&&) - both true"
fi

if "$CJSH_PATH" -c '[[ "hello" == "hello" && "world" == "foo" ]]'; then
    # Should fail because second condition is false
    fail_test "double bracket logical AND (&&) - second false"
else
    pass_test "double bracket logical AND (&&) - second false"
fi

if "$CJSH_PATH" -c '[[ "hello" == "hello" || "world" == "foo" ]]'; then
    pass_test "double bracket logical OR (||) - first true"
else
    fail_test "double bracket logical OR (||) - first true"
fi

if "$CJSH_PATH" -c '[[ "hello" == "foo" || "world" == "bar" ]]'; then
    # Should fail because both conditions are false
    fail_test "double bracket logical OR (||) - both false"
else
    pass_test "double bracket logical OR (||) - both false"
fi

# Test complex expressions
if "$CJSH_PATH" -c '[[ "hello" == "hello" && 5 -gt 3 ]]' 2>/dev/null; then
    pass_test "double bracket complex expression"
else
    fail_test "double bracket complex expression"
fi

# Test negation
if "$CJSH_PATH" -c '[[ ! "hello" == "world" ]]'; then
    pass_test "double bracket negation with !"
else
    fail_test "double bracket negation with !"
fi

if "$CJSH_PATH" -c '[[ ! -f "/nonexistent/file" ]]'; then
    pass_test "double bracket negation of file test"
else
    fail_test "double bracket negation of file test"
fi

# Test edge cases
if "$CJSH_PATH" -c '[[ "" ]]'; then
    # Empty string should be false
    fail_test "double bracket empty string truthiness"
else
    pass_test "double bracket empty string truthiness"
fi

if "$CJSH_PATH" -c '[[ "nonempty" ]]'; then
    pass_test "double bracket non-empty string truthiness"
else
    fail_test "double bracket non-empty string truthiness"
fi

# Test with variables
VAR1="hello"
VAR2="world"
export VAR1 VAR2

if "$CJSH_PATH" -c '[[ "$VAR1" == "hello" ]]'; then
    pass_test "double bracket with variable expansion"
else
    fail_test "double bracket with variable expansion"
fi

if "$CJSH_PATH" -c '[[ "$VAR1$VAR2" == "helloworld" ]]'; then
    pass_test "double bracket with concatenated variables"
else
    fail_test "double bracket with concatenated variables"
fi

# Test arithmetic expressions in double brackets
if "$CJSH_PATH" -c '[[ $((5 + 3)) -eq 8 ]]'; then
    pass_test "double bracket with arithmetic expansion"
else
    fail_test "double bracket with arithmetic expansion"
fi

# Test with special characters and escaping
if "$CJSH_PATH" -c '[[ "hello world" == "hello world" ]]'; then
    pass_test "double bracket with spaces in strings"
else
    fail_test "double bracket with spaces in strings"
fi

if "$CJSH_PATH" -c '[[ "hello\ttab" == "hello\ttab" ]]'; then
    pass_test "double bracket with escaped characters"
else
    fail_test "double bracket with escaped characters"
fi

# Test invalid expressions (should fail gracefully)
if "$CJSH_PATH" -c '[[ abc -invalidop def ]]' 2>/dev/null; then
    fail_test "double bracket invalid operator handling"
else
    pass_test "double bracket invalid operator handling"
fi

# Test numeric comparison edge cases
if "$CJSH_PATH" -c '[[ 0 -eq 0 ]]'; then
    pass_test "double bracket zero equality"
else
    fail_test "double bracket zero equality"
fi

if "$CJSH_PATH" -c '[[ -5 -lt 0 ]]'; then
    pass_test "double bracket negative number comparison"
else
    fail_test "double bracket negative number comparison"
fi

# Test with mixed string/numeric (should handle gracefully)
if "$CJSH_PATH" -c '[[ "abc" -eq 123 ]]' 2>/dev/null; then
    fail_test "double bracket string/number type mismatch"
else
    pass_test "double bracket string/number type mismatch"
fi

# Clean up temporary files
rm -f "$TEMP_FILE"
rm -rf "$TEMP_DIR"

echo
echo "Double Bracket Test Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "All double bracket tests passed!"
    exit 0
else
    echo "Some double bracket tests failed!"
    exit 1
fi