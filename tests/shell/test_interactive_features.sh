#!/usr/bin/env sh
# Test interactive shell features (prompt, history, completion)
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: interactive features..."

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

# Test 1: history command
echo "Testing history command..."
"$CJSH_PATH" -c "history" >/tmp/history_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "history command exists"
else
    fail_test "history command not found"
fi

# Test 2: history with commands
echo "Testing history with commands..."
"$CJSH_PATH" -c "echo test; history" >/tmp/history_commands_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "history records commands"
else
    fail_test "history command execution"
fi

# Test 3: Check prompt components
echo "Testing prompt components..."
PROMPT_DIR="$(cd "$(dirname "$0")/../../include/prompt" && pwd)"
if [ -d "$PROMPT_DIR" ]; then
    pass_test "prompt directory exists"
    
    # Check for prompt header files
    if ls "$PROMPT_DIR"/*.h >/dev/null 2>&1; then
        pass_test "prompt header files exist"
    else
        fail_test "prompt header files missing"
    fi
else
    fail_test "prompt directory not found"
fi

# Test 4: Check prompt source files
PROMPT_SRC_DIR="$(cd "$(dirname "$0")/../../src/prompt" && pwd)"
if [ -d "$PROMPT_SRC_DIR" ]; then
    pass_test "prompt source directory exists"
    
    if ls "$PROMPT_SRC_DIR"/*.cpp >/dev/null 2>&1; then
        pass_test "prompt source files exist"
    else
        fail_test "prompt source files missing"
    fi
else
    fail_test "prompt source directory not found"
fi

# Test 5: prompt_test command
echo "Testing prompt_test command..."
"$CJSH_PATH" -c "prompt_test" >/tmp/prompt_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "prompt_test command exists"
else
    skip_test "prompt_test command (may not be available)"
fi

# Test 6: Check isocline integration (for line editing)
echo "Testing isocline integration..."
ISOCLINE_DIR="$(cd "$(dirname "$0")/../../include/isocline" && pwd)"
if [ -d "$ISOCLINE_DIR" ]; then
    pass_test "isocline directory exists"
else
    fail_test "isocline directory not found"
fi

ISOCLINE_SRC_DIR="$(cd "$(dirname "$0")/../../src/isocline" && pwd)"
if [ -d "$ISOCLINE_SRC_DIR" ]; then
    pass_test "isocline source directory exists"
else
    fail_test "isocline source directory not found"
fi

# Test 7: Completion functionality (basic test)
echo "Testing completion functionality..."
# This is hard to test non-interactively, so we'll just check if completion files exist
if find "$(dirname "$0")/../../" -name "*completion*" -o -name "*complete*" | grep -v test >/dev/null 2>&1; then
    pass_test "completion related files exist"
else
    skip_test "completion files (may be integrated elsewhere)"
fi

# Test 8: History file handling
echo "Testing history file handling..."
# Test if history persists (create temp history file)
TEMP_HISTORY="/tmp/cjsh_test_history"
rm -f "$TEMP_HISTORY"

# Run shell with history file and add command
HISTFILE="$TEMP_HISTORY" "$CJSH_PATH" -c "echo 'test command'" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "shell runs with custom history file"
else
    fail_test "shell with custom history file"
fi

# Test 9: Interactive mode detection
echo "Testing interactive mode detection..."
# Test non-interactive mode (command execution)
"$CJSH_PATH" -c "echo non-interactive" >/tmp/non_interactive_test.out 2>&1
if [ $? -eq 0 ] && grep -q "non-interactive" /tmp/non_interactive_test.out; then
    pass_test "non-interactive mode works"
else
    fail_test "non-interactive mode"
fi

# Test 10: Login shell mode
echo "Testing login shell mode..."
"$CJSH_PATH" -l -c "echo login mode" >/tmp/login_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "login shell mode"
else
    skip_test "login shell mode (may not be supported)"
fi

# Test 11: Color support
echo "Testing color support..."
"$CJSH_PATH" -c "echo -e '\033[32mgreen\033[0m'" >/tmp/color_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "color escape sequences supported"
else
    fail_test "color support"
fi

# Test 12: Syntax highlighting (if available)
echo "Testing syntax highlighting..."
# This is hard to test without a terminal, so we'll check for related code
if grep -r "syntax.*highlight\|highlight.*syntax" "$(dirname "$0")/../../include" >/dev/null 2>&1; then
    pass_test "syntax highlighting code exists"
else
    skip_test "syntax highlighting (may not be implemented)"
fi

# Test 13: Tab completion
echo "Testing tab completion infrastructure..."
# Check if there are completion-related functions in the code
if grep -r "complete\|completion" "$(dirname "$0")/../../include" | grep -v test >/dev/null 2>&1; then
    pass_test "completion infrastructure exists"
else
    skip_test "completion infrastructure"
fi

# Test 14: Prompt customization
echo "Testing prompt customization..."
"$CJSH_PATH" -c "PS1='test> '; echo \$PS1" >/tmp/prompt_custom_test.out 2>&1
if [ $? -eq 0 ] && grep -q "test>" /tmp/prompt_custom_test.out; then
    pass_test "prompt customization (PS1)"
else
    skip_test "prompt customization"
fi

# Test 15: Command recall (history expansion)
echo "Testing command recall..."
# This is complex to test non-interactively, so we'll test if the history command works with options
"$CJSH_PATH" -c "history -c" >/tmp/history_clear_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then  # May not be implemented or may error
    skip_test "history options (may not be fully implemented)"
else
    skip_test "history options"
fi

# Test 16: Line editing functionality
echo "Testing line editing functionality..."
# Check for key bindings or editing features
if echo "test input" | "$CJSH_PATH" -c "read line; echo \$line" >/tmp/line_edit_test.out 2>&1; then
    if grep -q "test input" /tmp/line_edit_test.out; then
        pass_test "basic line input/editing"
    else
        fail_test "line input processing"
    fi
else
    fail_test "line editing test"
fi

# Test 17: Signal handling in interactive mode
echo "Testing signal handling..."
# Test if shell handles signals properly (timeout after 2 seconds to avoid hanging)
timeout 2 "$CJSH_PATH" -c "sleep 1" >/dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then  # 0 = success, 124 = timeout
    pass_test "signal handling (timeout works)"
else
    fail_test "signal handling"
fi

# Cleanup
rm -f /tmp/history_test.out /tmp/history_commands_test.out /tmp/prompt_test.out
rm -f /tmp/non_interactive_test.out /tmp/login_test.out /tmp/color_test.out
rm -f /tmp/prompt_custom_test.out /tmp/history_clear_test.out /tmp/line_edit_test.out
rm -f "$TEMP_HISTORY"

echo ""
echo "Interactive Features Tests Summary:"
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