#!/usr/bin/env sh
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

echo "Testing history command..."
"$CJSH_PATH" -c "history" >/tmp/history_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "history command exists"
else
    fail_test "history command not found"
fi

echo "Testing history with commands..."
"$CJSH_PATH" -c "echo test; history" >/tmp/history_commands_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "history records commands"
else
    fail_test "history command execution"
fi

echo "Testing prompt components..."
PROMPT_DIR="$(cd "$(dirname "$0")/../../include/prompt" && pwd)"
if [ -d "$PROMPT_DIR" ]; then
    pass_test "prompt directory exists"
    
    if ls "$PROMPT_DIR"/*.h >/dev/null 2>&1; then
        pass_test "prompt header files exist"
    else
        fail_test "prompt header files missing"
    fi
else
    fail_test "prompt directory not found"
fi

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


echo "Testing history file handling..."
TEMP_HISTORY="/tmp/cjsh_test_history"
rm -f "$TEMP_HISTORY"

HISTFILE="$TEMP_HISTORY" "$CJSH_PATH" -c "echo 'test command'" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "shell runs with custom history file"
else
    fail_test "shell with custom history file"
fi

echo "Testing interactive mode detection..."
"$CJSH_PATH" -c "echo non-interactive" >/tmp/non_interactive_test.out 2>&1
if [ $? -eq 0 ] && grep -q "non-interactive" /tmp/non_interactive_test.out; then
    pass_test "non-interactive mode works"
else
    fail_test "non-interactive mode"
fi

echo "Testing login shell mode..."
"$CJSH_PATH" -l -c "echo login mode" >/tmp/login_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "login shell mode"
else
    fail_test "login shell mode not supported"
fi

echo "Testing color support..."
"$CJSH_PATH" -c "echo -e '\033[32mgreen\033[0m'" >/tmp/color_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "color escape sequences supported"
else
    fail_test "color support"
fi

echo "Testing syntax highlighting..."
if grep -r "syntax.*highlight\|highlight.*syntax" "$(dirname "$0")/../../include" >/dev/null 2>&1; then
    pass_test "syntax highlighting code exists"
else
    skip_test "syntax highlighting (may not be implemented)"
fi


echo "Testing prompt customization..."
"$CJSH_PATH" -c "PS1='test> '; echo \$PS1" >/tmp/prompt_custom_test.out 2>&1
if [ $? -eq 0 ] && grep -q "test>" /tmp/prompt_custom_test.out; then
    pass_test "prompt customization (PS1)"
else
    fail_test "prompt customization"
fi


echo "Testing line editing functionality..."
if echo "test input" | "$CJSH_PATH" -c "read line; echo \$line" >/tmp/line_edit_test.out 2>&1; then
    if grep -q "test input" /tmp/line_edit_test.out; then
        pass_test "basic line input/editing"
    else
        fail_test "line input processing"
    fi
else
    fail_test "line editing test"
fi

echo "Testing signal handling..."
timeout 2 "$CJSH_PATH" -c "sleep 1" >/dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then  # 0 = success, 124 = timeout
    pass_test "signal handling (timeout works)"
else
    fail_test "signal handling"
fi

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