#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: brace expansion..."
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
OUT=$("$CJSH_PATH" -c "echo {1..3}" 2>&1)
if [ "$OUT" = "1 2 3" ]; then
    pass_test "integer range 1..3"
else
    fail_test "integer range 1..3 (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {5..1}" 2>&1)
if [ "$OUT" = "5 4 3 2 1" ]; then
    pass_test "integer range 5..1"
else
    fail_test "integer range 5..1 (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {0..0}" 2>&1)
if [ "$OUT" = "0" ]; then
    pass_test "single digit range 0..0"
else
    fail_test "single digit range 0..0 (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a..c}" 2>&1)
if [ "$OUT" = "a b c" ]; then
    pass_test "character range a..c"
else
    fail_test "character range a..c (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {z..x}" 2>&1)
if [ "$OUT" = "z y x" ]; then
    pass_test "character range z..x"
else
    fail_test "character range z..x (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {A..C}" 2>&1)
if [ "$OUT" = "A B C" ]; then
    pass_test "uppercase character range A..C"
else
    fail_test "uppercase character range A..C (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a..a}" 2>&1)
if [ "$OUT" = "a" ]; then
    pass_test "single character range a..a"
else
    fail_test "single character range a..a (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {foo,bar,baz}" 2>&1)
if [ "$OUT" = "foo bar baz" ]; then
    pass_test "comma-separated {foo,bar,baz}"
else
    fail_test "comma-separated {foo,bar,baz} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a,,c}" 2>&1)
if [ "$OUT" = "a c" ]; then
    pass_test "comma with empty element {a,,c} (bash-like)"
else
    fail_test "comma with empty element {a,,c} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo file{1..3}.txt" 2>&1)
if [ "$OUT" = "file1.txt file2.txt file3.txt" ]; then
    pass_test "range with prefix/suffix file{1..3}.txt"
else
    fail_test "range with prefix/suffix file{1..3}.txt (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo test{a..c}.log" 2>&1)
if [ "$OUT" = "testa.log testb.log testc.log" ]; then
    pass_test "character range with prefix/suffix test{a..c}.log"
else
    fail_test "character range with prefix/suffix test{a..c}.log (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo pre{one,two,three}post" 2>&1)
if [ "$OUT" = "preonepost pretwopost prethreepost" ]; then
    pass_test "comma expansion with prefix/suffix"
else
    fail_test "comma expansion with prefix/suffix (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a,b}{1,2}" 2>&1)
if [ "$OUT" = "a1 a2 b1 b2" ]; then
    pass_test "nested braces {a,b}{1,2}"
else
    fail_test "nested braces {a,b}{1,2} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {1..2} and {a..b}" 2>&1)
if [ "$OUT" = "1 2 and a b" ]; then
    pass_test "multiple separate brace expansions"
else
    fail_test "multiple separate brace expansions (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {-2..0}" 2>&1)
if [ "$OUT" = "-2 -1 0" ]; then
    pass_test "negative integer range {-2..0}"
else
    fail_test "negative integer range {-2..0} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {1..5}" 2>&1)
if [ "$OUT" = "1 2 3 4 5" ]; then
    pass_test "moderate integer range {1..5}"
else
    fail_test "moderate integer range {1..5} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a..3}" 2>&1)
if [ "$OUT" = "{a..3}" ]; then
    pass_test "invalid mixed range {a..3} returns literal"
else
    fail_test "invalid mixed range {a..3} should return literal (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {}" 2>&1)
if [ "$OUT" = "{}" ]; then
    pass_test "empty braces {} return literal"
else
    fail_test "empty braces {} should return literal (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {,,,}" 2>&1)
if [ "$OUT" = "" ]; then
    pass_test "comma-only braces {,,,} (bash-like - filtered)"
else
    fail_test "comma-only braces {,,,} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {1..3" 2>&1)
if [ "$OUT" = "{1..3" ]; then
    pass_test "unmatched opening brace returns literal"
else
    fail_test "unmatched opening brace should return literal (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo '{1..3}'" 2>&1)
if [ "$OUT" = "{1..3}" ]; then
    pass_test "quoted braces do not expand"
else
    fail_test "quoted braces should not expand (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo '{1..3}'" 2>&1)
if [ "$OUT" = "{1..3}" ]; then
    pass_test "quoted braces do not expand (alternative test)"
else
    fail_test "quoted braces should not expand (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='{1..3}'; echo \$var" 2>&1)
if [ "$OUT" = "{1..3}" ]; then
    pass_test "range in variable assignment stays literal"
else
    fail_test "range in variable assignment (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a,b{1,2},c}" 2>&1)
if [ "$OUT" = "a b1 b2 c" ]; then
    pass_test "complex nested expansion {a,b{1,2},c}"
else
    fail_test "complex nested expansion {a,b{1,2},c} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {01..03}" 2>&1)
if [ "$OUT" = "1 2 3" ]; then
    pass_test "range with leading zeros {01..03}"
else
    fail_test "range with leading zeros {01..03} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {z..A}" 2>&1)
if [ "$OUT" = "{z..A}" ]; then
    pass_test "cross-case range {z..A} returns literal"
else
    fail_test "cross-case range {z..A} should return literal (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {0..2}" 2>&1)
if [ "$OUT" = "0 1 2" ]; then
    pass_test "zero-based range {0..2}"
else
    fail_test "zero-based range {0..2} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {a..e}" 2>&1)
if [ "$OUT" = "a b c d e" ]; then
    pass_test "larger character range {a..e}"
else
    fail_test "larger character range {a..e} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo {10..12}" 2>&1)
if [ "$OUT" = "10 11 12" ]; then
    pass_test "double-digit range {10..12}"
else
    fail_test "double-digit range {10..12} (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "echo \$(echo {1..2})" 2>&1)
if [ "$OUT" = "1 2" ]; then
    pass_test "range in command substitution"
else
    fail_test "range in command substitution (got: '$OUT')"
fi
echo ""
echo "Brace Expansion Tests Summary:"
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
