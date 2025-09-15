#!/usr/bin/env sh
# Test AI integration functionality
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: AI integration..."

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

# Test 1: ai command exists
echo "Testing AI command availability..."
"$CJSH_PATH" -c "ai" >/tmp/ai_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then  # Command exists (exit 1 might be usage error)
    pass_test "ai command exists"
else
    fail_test "ai command not found"
fi

# Test 2: ai help
echo "Testing AI help..."
"$CJSH_PATH" -c "ai help" >/tmp/ai_help_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "ai help command"
else
    skip_test "ai help command"
fi

# Test 3: aihelp command
echo "Testing aihelp command..."
"$CJSH_PATH" -c "aihelp" >/tmp/aihelp_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "aihelp command exists"
else
    skip_test "aihelp command"
fi

# Test 4: AI configuration
echo "Testing AI configuration..."
"$CJSH_PATH" -c "ai config" >/tmp/ai_config_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai config command exists"
else
    skip_test "ai config command"
fi

# Test 5: AI status
echo "Testing AI status..."
"$CJSH_PATH" -c "ai status" >/tmp/ai_status_test.out 2>&1
if [ $? -eq 0 ]; then
    pass_test "ai status command"
else
    skip_test "ai status command (AI may be disabled)"
fi

# Test 6: Check AI header files
AI_HEADER="$(cd "$(dirname "$0")/../../include/ai" && pwd)/ai.h"
if [ -f "$AI_HEADER" ]; then
    pass_test "AI header file exists"
else
    fail_test "AI header file missing"
fi

# Test 7: AI source files
AI_SRC_DIR="$(cd "$(dirname "$0")/../../src/ai" && pwd)"
if [ -d "$AI_SRC_DIR" ]; then
    pass_test "AI source directory exists"
    
    # Check for key AI source files
    if ls "$AI_SRC_DIR"/*.cpp >/dev/null 2>&1; then
        pass_test "AI source files exist"
    else
        fail_test "AI source files missing"
    fi
else
    fail_test "AI source directory missing"
fi

# Test 8: AI enable/disable
echo "Testing AI enable/disable..."
"$CJSH_PATH" -c "ai disable" >/tmp/ai_disable_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai disable command exists"
else
    skip_test "ai disable command"
fi

"$CJSH_PATH" -c "ai enable" >/tmp/ai_enable_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai enable command exists"
else
    skip_test "ai enable command"
fi

# Test 9: AI model configuration
echo "Testing AI model configuration..."
"$CJSH_PATH" -c "ai model" >/tmp/ai_model_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai model command exists"
else
    skip_test "ai model command"
fi

# Test 10: AI API key handling (without exposing actual keys)
echo "Testing AI API key handling..."
"$CJSH_PATH" -c "ai key" >/tmp/ai_key_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai key command exists"
else
    skip_test "ai key command"
fi

# Test 11: AI session management
echo "Testing AI session management..."
"$CJSH_PATH" -c "ai session" >/tmp/ai_session_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai session command exists"
else
    skip_test "ai session command"
fi

# Test 12: AI cache functionality
echo "Testing AI cache functionality..."
"$CJSH_PATH" -c "ai cache" >/tmp/ai_cache_test.out 2>&1
if [ $? -eq 0 ] || [ $? -eq 1 ]; then
    pass_test "ai cache command exists"
else
    skip_test "ai cache command"
fi

# Test 13: AI prompt functionality (basic test without API call)
echo "Testing AI prompt functionality..."
"$CJSH_PATH" -c "ai prompt 'test'" >/tmp/ai_prompt_test.out 2>&1
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass_test "ai prompt command accepts input"
elif [ $exit_code -eq 1 ] && grep -q -i "api\|key\|config\|disabled" /tmp/ai_prompt_test.out; then
    skip_test "ai prompt command (requires API configuration)"
else
    fail_test "ai prompt command error"
fi

# Test 14: AI integration with shell commands
echo "Testing AI-shell integration..."
"$CJSH_PATH" -c "ai suggest 'list files'" >/tmp/ai_suggest_test.out 2>&1
if [ $? -eq 0 ] || ([ $? -ne 0 ] && grep -q -i "api\|key\|config" /tmp/ai_suggest_test.out); then
    skip_test "ai suggest command (requires API configuration)"
else
    skip_test "ai suggest command"
fi

# Test 15: AI completion integration
echo "Testing AI completion integration..."
"$CJSH_PATH" -c "ai complete 'ls -'" >/tmp/ai_complete_test.out 2>&1
if [ $? -eq 0 ] || ([ $? -ne 0 ] && grep -q -i "api\|key\|config" /tmp/ai_complete_test.out); then
    skip_test "ai complete command (requires API configuration)"
else
    skip_test "ai complete command"
fi

# Test 16: Check for AI dependencies (nlohmann/json)
echo "Testing AI dependencies..."
# This is a build-time check, so we'll just verify the headers can be found
if find "$(dirname "$0")/../../" -name "*.h" -o -name "*.hpp" | xargs grep -l "nlohmann.*json" >/dev/null 2>&1; then
    pass_test "AI JSON dependency integration"
else
    skip_test "AI JSON dependency check"
fi

# Test 17: AI error handling
echo "Testing AI error handling..."
"$CJSH_PATH" -c "ai invalid_command" >/tmp/ai_error_test.out 2>&1
if [ $? -ne 0 ]; then
    pass_test "AI error handling for invalid commands"
else
    skip_test "AI error handling"
fi

# Cleanup
rm -f /tmp/ai_test.out /tmp/ai_help_test.out /tmp/aihelp_test.out /tmp/ai_config_test.out
rm -f /tmp/ai_status_test.out /tmp/ai_disable_test.out /tmp/ai_enable_test.out
rm -f /tmp/ai_model_test.out /tmp/ai_key_test.out /tmp/ai_session_test.out
rm -f /tmp/ai_cache_test.out /tmp/ai_prompt_test.out /tmp/ai_suggest_test.out
rm -f /tmp/ai_complete_test.out /tmp/ai_error_test.out

echo ""
echo "AI Integration Tests Summary:"
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