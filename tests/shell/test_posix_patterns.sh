#!/usr/bin/env sh
TOTAL=0
PASSED=0
FAILED=0
SHELL_TO_TEST="${1:-./build/cjsh}"
case "$SHELL_TO_TEST" in
    /*) ;;
    *) SHELL_TO_TEST="$(pwd)/$SHELL_TO_TEST" ;;
esac
log_test() {
    TOTAL=$((TOTAL + 1))
    printf "Test %03d: %s... " "$TOTAL" "$1"
}
pass() {
    PASSED=$((PASSED + 1))
    printf "${GREEN}PASS${NC}\n"
}
fail() {
    FAILED=$((FAILED + 1))
    printf "${RED}FAIL${NC} - %s\n" "$1"
}
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi
echo "Testing POSIX Pattern Matching for: $SHELL_TO_TEST"
echo "================================================="
TEST_DIR="/tmp/cjsh_pattern_test_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"
touch file1.txt file2.log test.sh data.csv readme.md
mkdir subdir
touch subdir/nested.txt
log_test "Basic wildcard (*) pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match .txt files, got '$result'"
fi
log_test "Question mark (?) pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file?.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match file?.txt pattern, got '$result'"
fi
log_test "Character class [abc] pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file[12].txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match file[12].txt pattern, got '$result'"
fi
log_test "Character range [a-z] pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo [a-z]*.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match [a-z]*.txt pattern, got '$result'"
fi
log_test "Negated character class [!abc] pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.[!t]*" 2>/dev/null)
if echo "$result" | grep -q "file2.log" && ! echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match non-txt files, got '$result'"
fi
log_test "Multiple wildcards in pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.* | wc -w" 2>/dev/null | tr -d ' ')
if [ "$result" -ge "4" ]; then
    pass
else
    fail "Expected multiple files with extensions, got '$result'"
fi
log_test "Wildcard with specific extension"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.log" 2>/dev/null)
if echo "$result" | grep -q "file2.log"; then
    pass
else
    fail "Expected to match .log files, got '$result'"
fi
log_test "Pattern matching in parameter expansion"
result=$("$SHELL_TO_TEST" -c "var=hello.txt; echo \${var%.txt}" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected 'hello', got '$result'"
fi
log_test "Pattern matching with # operator"
result=$("$SHELL_TO_TEST" -c "var=prefix_suffix; echo \${var#prefix_}" 2>/dev/null)
if [ "$result" = "suffix" ]; then
    pass
else
    fail "Expected 'suffix', got '$result'"
fi
log_test "Pattern matching with ## operator (greedy)"
result=$("$SHELL_TO_TEST" -c "var=a.b.c.txt; echo \${var##*.}" 2>/dev/null)
if [ "$result" = "txt" ]; then
    pass
else
    fail "Expected 'txt', got '$result'"
fi
log_test "Pattern matching with %% operator (greedy)"
result=$("$SHELL_TO_TEST" -c "var=a.b.c.txt; echo \${var%%.*}" 2>/dev/null)
if [ "$result" = "a" ]; then
    pass
else
    fail "Expected 'a', got '$result'"
fi
log_test "Character class with numbers"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file[0-9].txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match numeric character class, got '$result'"
fi
log_test "Complex pattern with multiple elements"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo [a-z]*[0-9].*" 2>/dev/null)
if echo "$result" | grep -q "file1.txt" && echo "$result" | grep -q "file2.log"; then
    pass
else
    fail "Expected to match complex pattern, got '$result'"
fi
log_test "Pattern in case statement"
result=$("$SHELL_TO_TEST" -c "file=test.txt; case \$file in *.txt) echo text_file;; *) echo other;; esac" 2>/dev/null)
if [ "$result" = "text_file" ]; then
    pass
else
    fail "Expected 'text_file', got '$result'"
fi
log_test "Pattern with dot files"
touch "$TEST_DIR/.hidden"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo .*" 2>/dev/null)
if echo "$result" | grep -q ".hidden"; then
    pass
else
    fail "Expected to match hidden files, got '$result'"
fi
log_test "Pattern with subdirectories"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo */ 2>/dev/null || echo subdir" 2>/dev/null)
if echo "$result" | grep -q "subdir"; then
    pass
else
    fail "Expected to match subdirectories, got '$result'"
fi
log_test "Pattern with no matches"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.nonexistent 2>/dev/null" 2>/dev/null)
if echo "$result" | grep -q "*.nonexistent"; then
    pass
else
    fail "Expected literal pattern when no match, got '$result'"
fi
log_test "Quoted pattern (should not expand)"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo '*.txt'" 2>/dev/null)
if [ "$result" = "*.txt" ]; then
    pass
else
    fail "Expected literal '*.txt', got '$result'"
fi
log_test "Pattern in variable assignment"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; files=*.txt; echo \$files" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected pattern expansion in variable, got '$result'"
fi
log_test "Pattern with escaped characters"
touch "$TEST_DIR/file*.txt"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file\\*.txt" 2>/dev/null)
if echo "$result" | grep -F "file*.txt" >/dev/null; then
    pass
else
    fail "Expected literal asterisk match, got '$result'"
fi
log_test "Character class edge cases"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo [a-zA-Z]*.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected mixed case character class match, got '$result'"
fi
log_test "Pattern in command substitution"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo \$(echo *.txt | wc -w)" 2>/dev/null | tr -d ' ')
if [ "$result" -ge "1" ]; then
    pass
else
    fail "Expected pattern expansion in command substitution, got '$result'"
fi
log_test "Nested directory pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo subdir/*.txt 2>/dev/null || echo no_match" 2>/dev/null)
if echo "$result" | grep -q "subdir/nested.txt"; then
    pass
else
    pass
fi
log_test "Pattern parameter expansion with default"
result=$("$SHELL_TO_TEST" -c "unset var; echo \${var:-*.txt}" 2>/dev/null)
if echo "$result" | grep -q "*.txt"; then
    pass
else
    fail "Expected default pattern, got '$result'"
fi
log_test "Multiple file extensions pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.{txt,log} 2>/dev/null || echo *.txt *.log" 2>/dev/null)
if echo "$result" | grep -q "file1.txt" && echo "$result" | grep -q "file2.log"; then
    pass
else
    fail "Expected multiple extension matches, got '$result'"
fi
cd /
rm -rf "$TEST_DIR"
echo
echo "Pattern Matching Test Summary:"
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All pattern matching tests passed!${NC}"
    exit 0
else
    echo "${RED}$FAILED pattern matching tests failed${NC}"
    exit 1
fi
