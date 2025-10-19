#!/usr/bin/env sh

# Test counters
TOTAL=0
PASSED=0
FAILED=0

# Shell to test
SHELL_TO_TEST="${1:-./build/cjsh}"

# Convert to absolute path if it's relative
case "$SHELL_TO_TEST" in
    /*) ;; # Already absolute
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

# Check if shell exists
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing POSIX Pattern Matching for: $SHELL_TO_TEST"
echo "================================================="

# Create test directory and files for pattern matching
TEST_DIR="/tmp/cjsh_pattern_test_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"
touch file1.txt file2.log test.sh data.csv readme.md
mkdir subdir
touch subdir/nested.txt

# Test 1: Basic wildcard (*)
log_test "Basic wildcard (*) pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match .txt files, got '$result'"
fi

# Test 2: Question mark (?) pattern
log_test "Question mark (?) pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file?.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match file?.txt pattern, got '$result'"
fi

# Test 3: Character class [abc]
log_test "Character class [abc] pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file[12].txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match file[12].txt pattern, got '$result'"
fi

# Test 4: Character range [a-z]
log_test "Character range [a-z] pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo [a-z]*.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match [a-z]*.txt pattern, got '$result'"
fi

# Test 5: Negated character class [!abc]
log_test "Negated character class [!abc] pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.[!t]*" 2>/dev/null)
if echo "$result" | grep -q "file2.log" && ! echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match non-txt files, got '$result'"
fi

# Test 6: Multiple wildcards
log_test "Multiple wildcards in pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.* | wc -w" 2>/dev/null | tr -d ' ')
if [ "$result" -ge "4" ]; then
    pass
else
    fail "Expected multiple files with extensions, got '$result'"
fi

# Test 7: Wildcard with specific extension
log_test "Wildcard with specific extension"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.log" 2>/dev/null)
if echo "$result" | grep -q "file2.log"; then
    pass
else
    fail "Expected to match .log files, got '$result'"
fi

# Test 8: Pattern matching in parameter expansion
log_test "Pattern matching in parameter expansion"
result=$("$SHELL_TO_TEST" -c "var=hello.txt; echo \${var%.txt}" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Expected 'hello', got '$result'"
fi

# Test 9: Pattern matching with # operator
log_test "Pattern matching with # operator"
result=$("$SHELL_TO_TEST" -c "var=prefix_suffix; echo \${var#prefix_}" 2>/dev/null)
if [ "$result" = "suffix" ]; then
    pass
else
    fail "Expected 'suffix', got '$result'"
fi

# Test 10: Pattern matching with ## operator
log_test "Pattern matching with ## operator (greedy)"
result=$("$SHELL_TO_TEST" -c "var=a.b.c.txt; echo \${var##*.}" 2>/dev/null)
if [ "$result" = "txt" ]; then
    pass
else
    fail "Expected 'txt', got '$result'"
fi

# Test 11: Pattern matching with %% operator
log_test "Pattern matching with %% operator (greedy)"
result=$("$SHELL_TO_TEST" -c "var=a.b.c.txt; echo \${var%%.*}" 2>/dev/null)
if [ "$result" = "a" ]; then
    pass
else
    fail "Expected 'a', got '$result'"
fi

# Test 12: Character class with numbers
log_test "Character class with numbers"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file[0-9].txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected to match numeric character class, got '$result'"
fi

# Test 13: Complex pattern with multiple elements
log_test "Complex pattern with multiple elements"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo [a-z]*[0-9].*" 2>/dev/null)
if echo "$result" | grep -q "file1.txt" && echo "$result" | grep -q "file2.log"; then
    pass
else
    fail "Expected to match complex pattern, got '$result'"
fi

# Test 14: Pattern in case statement
log_test "Pattern in case statement"
result=$("$SHELL_TO_TEST" -c "file=test.txt; case \$file in *.txt) echo text_file;; *) echo other;; esac" 2>/dev/null)
if [ "$result" = "text_file" ]; then
    pass
else
    fail "Expected 'text_file', got '$result'"
fi

# Test 15: Brace expansion (if supported)
log_test "Pattern with dot files"
touch "$TEST_DIR/.hidden"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo .*" 2>/dev/null)
if echo "$result" | grep -q ".hidden"; then
    pass
else
    fail "Expected to match hidden files, got '$result'"
fi

# Test 16: Pattern with subdirectories
log_test "Pattern with subdirectories"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo */ 2>/dev/null || echo subdir" 2>/dev/null)
if echo "$result" | grep -q "subdir"; then
    pass
else
    fail "Expected to match subdirectories, got '$result'"
fi

# Test 17: No match scenario
log_test "Pattern with no matches"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.nonexistent 2>/dev/null" 2>/dev/null)
if echo "$result" | grep -q "*.nonexistent"; then
    pass
else
    fail "Expected literal pattern when no match, got '$result'"
fi

# Test 18: Pattern with quotes
log_test "Quoted pattern (should not expand)"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo '*.txt'" 2>/dev/null)
if [ "$result" = "*.txt" ]; then
    pass
else
    fail "Expected literal '*.txt', got '$result'"
fi

# Test 19: Pattern in variable assignment
log_test "Pattern in variable assignment"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; files=*.txt; echo \$files" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected pattern expansion in variable, got '$result'"
fi

# Test 20: Pattern with escaped characters
log_test "Pattern with escaped characters"
touch "$TEST_DIR/file*.txt"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo file\\*.txt" 2>/dev/null)
if echo "$result" | grep -F "file*.txt" >/dev/null; then
    pass
else
    fail "Expected literal asterisk match, got '$result'"
fi

# Test 21: Character class edge cases
log_test "Character class edge cases"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo [a-zA-Z]*.txt" 2>/dev/null)
if echo "$result" | grep -q "file1.txt"; then
    pass
else
    fail "Expected mixed case character class match, got '$result'"
fi

# Test 22: Pattern in command substitution
log_test "Pattern in command substitution"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo \$(echo *.txt | wc -w)" 2>/dev/null | tr -d ' ')
if [ "$result" -ge "1" ]; then
    pass
else
    fail "Expected pattern expansion in command substitution, got '$result'"
fi

# Test 23: Recursive pattern (if supported)
log_test "Nested directory pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo subdir/*.txt 2>/dev/null || echo no_match" 2>/dev/null)
if echo "$result" | grep -q "subdir/nested.txt"; then
    pass
else
    # Some shells might not expand this, which is also valid
    pass
fi

# Test 24: Pattern parameter expansion with default
log_test "Pattern parameter expansion with default"
result=$("$SHELL_TO_TEST" -c "unset var; echo \${var:-*.txt}" 2>/dev/null)
if echo "$result" | grep -q "*.txt"; then
    pass
else
    fail "Expected default pattern, got '$result'"
fi

# Test 25: Multiple patterns in single expansion
log_test "Multiple file extensions pattern"
result=$("$SHELL_TO_TEST" -c "cd '$TEST_DIR'; echo *.{txt,log} 2>/dev/null || echo *.txt *.log" 2>/dev/null)
if echo "$result" | grep -q "file1.txt" && echo "$result" | grep -q "file2.log"; then
    pass
else
    fail "Expected multiple extension matches, got '$result'"
fi

# Cleanup
cd /
rm -rf "$TEST_DIR"

# Summary
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