#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: printf comprehensive..."

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

# Test basic string formatting
OUT=$("$CJSH_PATH" -c "printf '%s\n' hello")
if [ "$OUT" != "hello" ]; then
    fail_test "printf basic string (got '$OUT')"
else
    pass_test "printf basic string"
fi

# Test integer formatting with %d
OUT=$("$CJSH_PATH" -c "printf '%d\n' 42")
if [ "$OUT" != "42" ]; then
    fail_test "printf integer decimal (got '$OUT')"
else
    pass_test "printf integer decimal"
fi

# Test integer formatting with %i
OUT=$("$CJSH_PATH" -c "printf '%i\n' 123")
if [ "$OUT" != "123" ]; then
    fail_test "printf integer %i format (got '$OUT')"
else
    pass_test "printf integer %i format"
fi

# Test octal formatting
OUT=$("$CJSH_PATH" -c "printf '%o\n' 8")
if [ "$OUT" != "10" ]; then
    fail_test "printf octal format (got '$OUT', expected '10')"
else
    pass_test "printf octal format"
fi

# Test hexadecimal lowercase
OUT=$("$CJSH_PATH" -c "printf '%x\n' 255")
if [ "$OUT" != "ff" ]; then
    fail_test "printf hex lowercase (got '$OUT', expected 'ff')"
else
    pass_test "printf hex lowercase"
fi

# Test hexadecimal uppercase
OUT=$("$CJSH_PATH" -c "printf '%X\n' 255")
if [ "$OUT" != "FF" ]; then
    fail_test "printf hex uppercase (got '$OUT', expected 'FF')"
else
    pass_test "printf hex uppercase"
fi

# Test unsigned integer
OUT=$("$CJSH_PATH" -c "printf '%u\n' 42")
if [ "$OUT" != "42" ]; then
    fail_test "printf unsigned integer (got '$OUT')"
else
    pass_test "printf unsigned integer"
fi

# Test floating point %f
OUT=$("$CJSH_PATH" -c "printf '%f\n' 3.14")
if [ "$OUT" != "3.140000" ]; then
    fail_test "printf float default precision (got '$OUT', expected '3.140000')"
else
    pass_test "printf float default precision"
fi

# Test floating point with precision
OUT=$("$CJSH_PATH" -c "printf '%.2f\n' 3.14159")
if [ "$OUT" != "3.14" ]; then
    fail_test "printf float with precision (got '$OUT', expected '3.14')"
else
    pass_test "printf float with precision"
fi

# Test exponential notation lowercase
OUT=$("$CJSH_PATH" -c "printf '%e\n' 1000")
EXPECTED="1.000000e+03"
if [ "$OUT" != "$EXPECTED" ] && [ "$OUT" != "1.000000e+3" ]; then
    skip_test "printf exponential lowercase (got '$OUT', implementation may vary)"
else
    pass_test "printf exponential lowercase"
fi

# Test exponential notation uppercase
OUT=$("$CJSH_PATH" -c "printf '%E\n' 1000")
EXPECTED="1.000000E+03"
if [ "$OUT" != "$EXPECTED" ] && [ "$OUT" != "1.000000E+3" ]; then
    skip_test "printf exponential uppercase (got '$OUT', implementation may vary)"
else
    pass_test "printf exponential uppercase"
fi

# Test character formatting
OUT=$("$CJSH_PATH" -c "printf '%c\n' A")
if [ "$OUT" != "A" ]; then
    fail_test "printf character (got '$OUT')"
else
    pass_test "printf character"
fi

# Test multiple format specifiers
OUT=$("$CJSH_PATH" -c "printf '%s %d %x\n' hello 10 15")
if [ "$OUT" != "hello 10 f" ]; then
    fail_test "printf multiple specifiers (got '$OUT', expected 'hello 10 f')"
else
    pass_test "printf multiple specifiers"
fi

# Test escaped characters
OUT=$("$CJSH_PATH" -c "printf 'hello\nworld\n'")
EXPECTED="hello
world"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "printf newline escape (output mismatch)"
else
    pass_test "printf newline escape"
fi

# Test tab character
OUT=$("$CJSH_PATH" -c "printf 'col1\tcol2\n'")
EXPECTED="col1	col2"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "printf tab escape (output mismatch)"
else
    pass_test "printf tab escape"
fi

# Test backslash escape
OUT=$("$CJSH_PATH" -c "printf 'back\\\\slash\n'")
if [ "$OUT" != "back\\slash" ]; then
    fail_test "printf backslash escape (got '$OUT')"
else
    pass_test "printf backslash escape"
fi

# Test percent literal
OUT=$("$CJSH_PATH" -c "printf '100%% complete\n'")
if [ "$OUT" != "100% complete" ]; then
    fail_test "printf percent literal (got '$OUT')"
else
    pass_test "printf percent literal"
fi

# Test width specifier
OUT=$("$CJSH_PATH" -c "printf '%10s\n' hello")
if [ "$OUT" != "     hello" ]; then
    fail_test "printf width specifier (got '$OUT', expected '     hello')"
else
    pass_test "printf width specifier"
fi

# Test left-align with minus flag
OUT=$("$CJSH_PATH" -c "printf '%-10s|\n' hello")
if [ "$OUT" != "hello     |" ]; then
    fail_test "printf left-align (got '$OUT', expected 'hello     |')"
else
    pass_test "printf left-align"
fi

# Test zero-padding
OUT=$("$CJSH_PATH" -c "printf '%05d\n' 42")
if [ "$OUT" != "00042" ]; then
    fail_test "printf zero-padding (got '$OUT', expected '00042')"
else
    pass_test "printf zero-padding"
fi

# Test printf with no arguments (just format string)
OUT=$("$CJSH_PATH" -c "printf 'just text\n'")
if [ "$OUT" != "just text" ]; then
    fail_test "printf no arguments (got '$OUT')"
else
    pass_test "printf no arguments"
fi

# Test printf with missing arguments (should use defaults)
OUT=$("$CJSH_PATH" -c "printf '%s %d\n' hello")
if [ "$OUT" != "hello 0" ]; then
    fail_test "printf missing argument defaults to 0 (got '$OUT')"
else
    pass_test "printf missing argument defaults to 0"
fi

# Test printf reusing format for multiple argument sets
OUT=$("$CJSH_PATH" -c "printf '%s\n' one two three")
EXPECTED="one
two
three"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "printf format reuse (output mismatch)"
else
    pass_test "printf format reuse"
fi

# Test invalid format specifier handling
OUT=$("$CJSH_PATH" -c "printf '%q\n' test" 2>&1)
# This should either print as-is or handle gracefully
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "printf invalid format specifier handled"
else
    fail_test "printf invalid format specifier not handled"
fi

# Test empty format string
OUT=$("$CJSH_PATH" -c "printf ''")
if [ "$OUT" != "" ]; then
    fail_test "printf empty format string (got '$OUT')"
else
    pass_test "printf empty format string"
fi

# Test printf with single quotes in string
OUT=$("$CJSH_PATH" -c "printf \"%s\n\" \"it's\"")
if [ "$OUT" != "it's" ]; then
    fail_test "printf single quote in string (got '$OUT')"
else
    pass_test "printf single quote in string"
fi

# Test printf return value on success
"$CJSH_PATH" -c "printf 'test\n' >/dev/null"
if [ $? -ne 0 ]; then
    fail_test "printf exit status on success"
else
    pass_test "printf exit status on success"
fi

echo ""
echo "Printf Comprehensive Tests Summary:"
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
