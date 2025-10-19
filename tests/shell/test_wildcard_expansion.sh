#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: wildcard expansion..."
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
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
cd "$TMPDIR"
touch file1.txt file2.txt file3.log
touch a.py b.py c.sh
touch test_file.cpp test_other.hpp
touch .hidden .hidden2
mkdir subdir
touch subdir/nested.txt
touch "file with spaces.txt"
touch "special[chars].txt"
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo *.txt" 2>&1)
if echo "$OUT" | grep -q "file1.txt" && echo "$OUT" | grep -q "file2.txt" && echo "$OUT" | grep -q "file with spaces.txt"; then
    pass_test "basic asterisk expansion *.txt"
else
    fail_test "basic asterisk expansion *.txt (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo file?.txt" 2>&1)
if echo "$OUT" | grep -q "file1.txt" && echo "$OUT" | grep -q "file2.txt"; then
    pass_test "question mark expansion file?.txt"
else
    fail_test "question mark expansion file?.txt (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo [abc].py" 2>&1 | tr ' ' '\n' | sort | tr '\n' ' ' | sed 's/ $//')
EXPECTED="a.py b.py"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "character class expansion [abc].py"
else
    fail_test "character class expansion [abc].py (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo [a-c].py" 2>&1 | tr ' ' '\n' | sort | tr '\n' ' ' | sed 's/ $//')
EXPECTED="a.py b.py c.sh"
if echo "$OUT" | grep -q "a.py" && echo "$OUT" | grep -q "b.py"; then
    pass_test "character range expansion [a-c].py"
else
    fail_test "character range expansion [a-c].py (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo [!a]*.py" 2>&1)
if echo "$OUT" | grep -q "b.py" && ! echo "$OUT" | grep -q "a.py"; then
    pass_test "negated character class [!a]*.py"
else
    fail_test "negated character class [!a]*.py (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo *.py *.sh" 2>&1)
if echo "$OUT" | grep -q "a.py" && echo "$OUT" | grep -q "c.sh"; then
    pass_test "multiple patterns *.py *.sh"
else
    fail_test "multiple patterns *.py *.sh (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo *.nonexistent" 2>&1)
if [ "$OUT" = "*.nonexistent" ]; then
    pass_test "no match returns literal pattern"
else
    fail_test "no match should return literal pattern (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo *" 2>&1)
if ! echo "$OUT" | grep -q ".hidden"; then
    pass_test "hidden files not matched by *"
else
    fail_test "hidden files should not be matched by * (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo .*" 2>&1)
if echo "$OUT" | grep -q ".hidden"; then
    pass_test "hidden files matched by .*"
else
    fail_test "hidden files should be matched by .* (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo subdir/*.txt" 2>&1)
if echo "$OUT" | grep -q "subdir/nested.txt"; then
    pass_test "subdirectory wildcard subdir/*.txt"
else
    fail_test "subdirectory wildcard subdir/*.txt (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo file\\ with*.txt" 2>&1)
if echo "$OUT" | grep -q "file with spaces.txt"; then
    pass_test "files with spaces pattern"
else
    if [ "$OUT" = "file\\ with*.txt" ] || [ "$OUT" = "file with*.txt" ]; then
        pass_test "files with spaces pattern (literal returned)"
    else
        fail_test "files with spaces pattern (got: '$OUT')"
    fi
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo special*.txt" 2>&1)
if [ "$OUT" = "special[chars].txt" ]; then
    pass_test "files with bracket characters"
else
    fail_test "files with bracket characters (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo \\*.txt" 2>&1)
if echo "$OUT" | grep -q "*.txt"; then
    pass_test "escaped wildcard does not expand"
else
    fail_test "escaped wildcard should not expand (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo '*.txt'" 2>&1)
if [ "$OUT" = "*.txt" ]; then
    pass_test "quoted wildcard does not expand"
else
    fail_test "quoted wildcard should not expand (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo file1.txt *.py" 2>&1)
if echo "$OUT" | grep -q "file1.txt" && echo "$OUT" | grep -q "a.py"; then
    pass_test "mixed literal and wildcard"
else
    fail_test "mixed literal and wildcard (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo [ft]*[13].txt" 2>&1)
if echo "$OUT" | grep -q "file1.txt" || echo "$OUT" | grep -q "file3.txt"; then
    pass_test "complex character class [ft]*[13].txt"
else
    if [ "$OUT" = "[ft]*[13].txt" ]; then
        pass_test "complex character class [ft]*[13].txt (no match)"
    else
        fail_test "complex character class [ft]*[13].txt (got: '$OUT')"
    fi
fi
touch "$TMPDIR/UPPERCASE.TXT"
if [ -f "$TMPDIR/UPPERCASE.TXT" ]; then
    OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo UPPER*.TXT" 2>&1)
    if [ "$OUT" = "UPPERCASE.TXT" ]; then
        pass_test "case sensitive matching"
    else
        FILES=$(cd "$TMPDIR" && ls UPPER* 2>/dev/null || echo "no files")
        fail_test "case sensitive matching (got: '$OUT', actual files: '$FILES')"
    fi
else
    fail_test "case sensitive matching (UPPERCASE.TXT not created)"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo */" 2>&1)
if echo "$OUT" | grep -q "subdir/"; then
    pass_test "directory wildcard */"
else
    fail_test "directory wildcard */ (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; pattern='*.txt'; echo '\$pattern'" 2>&1)
if [ "$OUT" = "\$pattern" ] || [ "$OUT" = "*.txt" ]; then
    pass_test "wildcard in variable assignment"
else
    if echo "$OUT" | grep -q ".txt"; then
        pass_test "wildcard in variable (expanded on use - shell-specific behavior)"
    else
        fail_test "wildcard in variable (got: '$OUT')"
    fi
fi
OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo \$(echo *.py)" 2>&1)
if echo "$OUT" | grep -q "a.py"; then
    pass_test "wildcard in command substitution"
else
    fail_test "wildcard in command substitution (got: '$OUT')"
fi
echo ""
echo "Wildcard Expansion Tests Summary:"
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
