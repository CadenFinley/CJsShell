#!/usr/bin/env sh
# Test: command substitution sentinel markers should not leak into output
# This test suite verifies that internal sentinel markers (__NOENV_START__, __NOENV_END__)
# used for protecting command substitution output from variable expansion are properly
# stripped and do not appear in final output

if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi

echo "Test: command substitution sentinel markers..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# Test 1: Simple echo with command substitution should not leak sentinels
RESULT=$("$CJSH_PATH" -c 'echo "$(echo hello)"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked in simple echo with command substitution (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked in simple echo with command substitution (got: $RESULT)"
elif [ "$RESULT" != "hello" ]; then
    fail_test "simple echo with command substitution output incorrect (got: $RESULT)"
else
    pass_test "simple echo with command substitution has no sentinel leakage"
fi

# Test 2: Echo with command substitution containing special characters
RESULT=$("$CJSH_PATH" -c 'echo "$(printf '"'"'/opt/homebrew'"'"')"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with path-like output (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with path-like output (got: $RESULT)"
elif [ "$RESULT" != "/opt/homebrew" ]; then
    fail_test "path-like output incorrect (got: $RESULT)"
else
    pass_test "path-like output has no sentinel leakage"
fi

# Test 3: Real-world brew --prefix scenario
# This was the original bug report case
RESULT=$("$CJSH_PATH" -c 'echo "source $(echo /opt/homebrew)/share/file.sh"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked in brew --prefix scenario (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked in brew --prefix scenario (got: $RESULT)"
elif [ "$RESULT" != "source /opt/homebrew/share/file.sh" ]; then
    fail_test "brew --prefix scenario output incorrect (got: $RESULT)"
else
    pass_test "brew --prefix scenario has no sentinel leakage"
fi

# Test 4: Command substitution with multiple words
RESULT=$("$CJSH_PATH" -c 'echo "prefix $(echo one two three) suffix"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked in multi-word output (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked in multi-word output (got: $RESULT)"
elif [ "$RESULT" != "prefix one two three suffix" ]; then
    fail_test "multi-word output incorrect (got: $RESULT)"
else
    pass_test "multi-word output has no sentinel leakage"
fi

# Test 5: Nested command substitutions
RESULT=$("$CJSH_PATH" -c 'echo "$(echo outer $(echo inner) end)"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked in nested command substitution (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked in nested command substitution (got: $RESULT)"
elif [ "$RESULT" != "outer inner end" ]; then
    fail_test "nested command substitution output incorrect (got: $RESULT)"
else
    pass_test "nested command substitution has no sentinel leakage"
fi

# Test 6: Command substitution with concatenated paths (simpler test)
RESULT=$("$CJSH_PATH" -c 'PREFIX=/usr/local; echo "$(echo $PREFIX)/bin"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with path concatenation (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with path concatenation (got: $RESULT)"
elif [ "$RESULT" != "/usr/local/bin" ]; then
    fail_test "path concatenation output incorrect (got: $RESULT)"
else
    pass_test "path concatenation has no sentinel leakage"
fi

# Test 7: Command substitution appended to file (redirection scenario)
TEMP_FILE="/tmp/cjsh_sentinel_test_$$"
"$CJSH_PATH" -c "echo \"source \$(echo /test/path)\" > $TEMP_FILE" 2>&1
if [ -f "$TEMP_FILE" ]; then
    FILE_CONTENT=$(cat "$TEMP_FILE")
    rm -f "$TEMP_FILE"
    if echo "$FILE_CONTENT" | grep -q "__NOENV_START__"; then
        fail_test "sentinels leaked in file redirection (got: $FILE_CONTENT)"
    elif echo "$FILE_CONTENT" | grep -q "__NOENV_END__"; then
        fail_test "sentinels leaked in file redirection (got: $FILE_CONTENT)"
    elif [ "$FILE_CONTENT" != "source /test/path" ]; then
        fail_test "file redirection output incorrect (got: $FILE_CONTENT)"
    else
        pass_test "file redirection has no sentinel leakage"
    fi
else
    fail_test "file redirection test - file not created"
fi

# Test 8: Command substitution in variable assignment then echo
RESULT=$("$CJSH_PATH" -c 'VAR="$(echo /usr/bin)"; echo "$VAR"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked in variable assignment (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked in variable assignment (got: $RESULT)"
elif [ "$RESULT" != "/usr/bin" ]; then
    fail_test "variable assignment output incorrect (got: $RESULT)"
else
    pass_test "variable assignment has no sentinel leakage"
fi

# Test 9: Command substitution with special shell variables
RESULT=$("$CJSH_PATH" -c 'echo "home=$(echo /home/user) path"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with variable-like names (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with variable-like names (got: $RESULT)"
elif [ "$RESULT" != "home=/home/user path" ]; then
    fail_test "output with variable-like names incorrect (got: $RESULT)"
else
    pass_test "output with variable-like names has no sentinel leakage"
fi

# Test 10: Command substitution with backticks (old style)
RESULT=$("$CJSH_PATH" -c 'echo "path=`echo /usr/local`"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with backticks (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with backticks (got: $RESULT)"
elif [ "$RESULT" != "path=/usr/local" ]; then
    fail_test "backtick output incorrect (got: $RESULT)"
else
    pass_test "backtick command substitution has no sentinel leakage"
fi

# Test 11: Multiple command substitutions in one line
RESULT=$("$CJSH_PATH" -c 'echo "$(echo first) and $(echo second) and $(echo third)"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with multiple substitutions (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with multiple substitutions (got: $RESULT)"
elif [ "$RESULT" != "first and second and third" ]; then
    fail_test "multiple substitutions output incorrect (got: $RESULT)"
else
    pass_test "multiple command substitutions have no sentinel leakage"
fi

# Test 12: Command substitution with slashes and underscores (brew prefix realistic case)
RESULT=$("$CJSH_PATH" -c 'echo "$(echo /opt/homebrew)/share/zsh-syntax-highlighting/file.zsh"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked in realistic brew case (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked in realistic brew case (got: $RESULT)"
elif [ "$RESULT" != "/opt/homebrew/share/zsh-syntax-highlighting/file.zsh" ]; then
    fail_test "realistic brew case output incorrect (got: $RESULT)"
else
    pass_test "realistic brew --prefix case has no sentinel leakage"
fi

# Test 13: Verify sentinels don't appear with printf builtin
RESULT=$("$CJSH_PATH" -c 'printf "%s\n" "$(echo test)"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with printf builtin (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with printf builtin (got: $RESULT)"
elif [ "$RESULT" != "test" ]; then
    fail_test "printf builtin output incorrect (got: $RESULT)"
else
    pass_test "printf builtin has no sentinel leakage"
fi

# Test 14: Command substitution in double quotes with dollar signs around it
RESULT=$("$CJSH_PATH" -c 'echo "$HOME $(echo test) $USER"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with surrounding variables (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with surrounding variables (got: $RESULT)"
else
    # We can't test exact output since HOME and USER vary, but no sentinel is the key
    pass_test "command substitution with surrounding variables has no sentinel leakage"
fi

# Test 15: Command substitution with >> append operator in output
RESULT=$("$CJSH_PATH" -c 'echo "$(echo command) >> file.log"' 2>&1)
if echo "$RESULT" | grep -q "__NOENV_START__"; then
    fail_test "sentinels leaked with >> in output (got: $RESULT)"
elif echo "$RESULT" | grep -q "__NOENV_END__"; then
    fail_test "sentinels leaked with >> in output (got: $RESULT)"
elif [ "$RESULT" != "command >> file.log" ]; then
    fail_test "output with >> incorrect (got: $RESULT)"
else
    pass_test "output containing >> has no sentinel leakage"
fi

# Print summary
echo ""
echo "================================"
echo "Test Summary:"
echo "  PASSED: $TESTS_PASSED"
echo "  FAILED: $TESTS_FAILED"
echo "================================"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
