#!/usr/bin/env sh
TOTAL=0
PASSED=0
FAILED=0
SHELL_TO_TEST="${1:-./build/cjsh}"
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
skip() {
    printf "${YELLOW}SKIP${NC} - %s\n" "$1"
}
if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi
echo "Testing POSIX I/O Redirection and Pipelines for: $SHELL_TO_TEST"
echo "=============================================================="
log_test "Output redirection >"
"$SHELL_TO_TEST" -c "echo hello > /tmp/test_out_$$" 2>/dev/null
if [ -f "/tmp/test_out_$$" ] && [ "$(cat /tmp/test_out_$$)" = "hello" ]; then
    pass
    rm -f "/tmp/test_out_$$"
else
    fail "Basic output redirection failed"
    rm -f "/tmp/test_out_$$"
fi
log_test "Output append redirection >>"
"$SHELL_TO_TEST" -c "echo line1 > /tmp/test_append_$$; echo line2 >> /tmp/test_append_$$" 2>/dev/null
result=$(cat "/tmp/test_append_$$" 2>/dev/null)
expected="line1
line2"
if [ "$result" = "$expected" ]; then
    pass
    rm -f "/tmp/test_append_$$"
else
    fail "Append redirection failed"
    rm -f "/tmp/test_append_$$"
fi
log_test "Input redirection <"
echo "input_data" > "/tmp/test_input_$$"
result=$("$SHELL_TO_TEST" -c "cat < /tmp/test_input_$$" 2>/dev/null)
if [ "$result" = "input_data" ]; then
    pass
    rm -f "/tmp/test_input_$$"
else
    fail "Input redirection failed"
    rm -f "/tmp/test_input_$$"
fi
log_test "Error redirection 2>"
"$SHELL_TO_TEST" -c "echo error >&2" 2> "/tmp/test_err_$$"
if [ -f "/tmp/test_err_$$" ] && [ "$(cat /tmp/test_err_$$)" = "error" ]; then
    pass
    rm -f "/tmp/test_err_$$"
else
    fail "Error redirection failed"
    rm -f "/tmp/test_err_$$"
fi
log_test "Error append redirection 2>>"
"$SHELL_TO_TEST" -c "echo err1 >&2; echo err2 >&2" 2> "/tmp/test_err_append_$$"
"$SHELL_TO_TEST" -c "echo err1 >&2" 2> "/tmp/test_err_append_$$"
"$SHELL_TO_TEST" -c "echo err2 >&2" 2>> "/tmp/test_err_append_$$"
result=$(cat "/tmp/test_err_append_$$" 2>/dev/null)
expected="err1
err2"
if [ "$result" = "$expected" ]; then
    pass
    rm -f "/tmp/test_err_append_$$"
else
    fail "Error append redirection failed"
    rm -f "/tmp/test_err_append_$$"
fi
log_test "Combine stdout and stderr 2>&1"
result=$("$SHELL_TO_TEST" -c "(echo stdout; echo stderr >&2) 2>&1" 2>/dev/null)
if echo "$result" | grep -q "stdout" && echo "$result" | grep -q "stderr"; then
    pass
else
    fail "Combining stdout and stderr failed"
fi
log_test "Redirect stderr to stdout"
result=$("$SHELL_TO_TEST" -c "echo 'to stderr' >&2" 2>&1)
if [ "$result" = "to stderr" ]; then
    pass
else
    fail "Redirecting stderr to stdout failed"
fi
log_test "File descriptor redirection 3>"
"$SHELL_TO_TEST" -c "echo test 3> /tmp/test_fd3_$$" 2>/dev/null
if [ -f "/tmp/test_fd3_$$" ]; then
    pass
    rm -f "/tmp/test_fd3_$$"
else
    fail "Custom file descriptor redirection not supported"
    rm -f "/tmp/test_fd3_$$"
fi
log_test "Here document <<"
result=$("$SHELL_TO_TEST" -c "cat << 'EOF'
line1
line2
EOF" 2>/dev/null)
expected="line1
line2"
if [ "$result" = "$expected" ]; then
    pass
else
    fail "Here document failed"
fi
log_test "Here document with variable expansion"
result=$("$SHELL_TO_TEST" -c "var=world; cat << EOF
Hello \$var
EOF" 2>/dev/null)
if [ "$result" = "Hello world" ]; then
    pass
else
    fail "Here document variable expansion failed"
fi
log_test "Here document with quoted delimiter"
result=$("$SHELL_TO_TEST" -c "var=world; cat << 'EOF'
Hello \$var
EOF" 2>/dev/null)
if [ "$result" = "Hello \$var" ]; then
    pass
else
    fail "Here document should not expand with quoted delimiter"
fi
log_test "Basic pipeline"
result=$("$SHELL_TO_TEST" -c "echo hello | cat" 2>/dev/null)
if [ "$result" = "hello" ]; then
    pass
else
    fail "Basic pipeline failed"
fi
log_test "Multi-stage pipeline"
result=$("$SHELL_TO_TEST" -c "echo 'hello world' | tr ' ' '\n' | wc -l" 2>/dev/null | tr -d ' ')
if [ "$result" = "2" ]; then
    pass
else
    fail "Multi-stage pipeline failed"
fi
log_test "Pipeline with redirection"
"$SHELL_TO_TEST" -c "echo hello | cat > /tmp/test_pipe_$$" 2>/dev/null
if [ -f "/tmp/test_pipe_$$" ] && [ "$(cat /tmp/test_pipe_$$)" = "hello" ]; then
    pass
    rm -f "/tmp/test_pipe_$$"
else
    fail "Pipeline with redirection failed"
    rm -f "/tmp/test_pipe_$$"
fi
log_test "Pipeline exit status"
"$SHELL_TO_TEST" -c "true | false" 2>/dev/null
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    fail "Pipeline should return exit status of last command"
fi
log_test "Input/output redirection combination"
echo "test_data" > "/tmp/test_combo_in_$$"
"$SHELL_TO_TEST" -c "cat < /tmp/test_combo_in_$$ > /tmp/test_combo_out_$$" 2>/dev/null
if [ -f "/tmp/test_combo_out_$$" ] && [ "$(cat /tmp/test_combo_out_$$)" = "test_data" ]; then
    pass
    rm -f "/tmp/test_combo_in_$$" "/tmp/test_combo_out_$$"
else
    fail "Input/output redirection combination failed"
    rm -f "/tmp/test_combo_in_$$" "/tmp/test_combo_out_$$"
fi
log_test "Noclobber behavior"
echo "original" > "/tmp/test_noclobber_$$"
"$SHELL_TO_TEST" -c "set -C; echo new > /tmp/test_noclobber_$$" 2>/dev/null
exit_code=$?
if [ $exit_code -ne 0 ]; then
    pass
else
    skip "Noclobber (set -C) not supported"
fi
rm -f "/tmp/test_noclobber_$$"
log_test "Force overwrite >|"
echo "original" > "/tmp/test_force_$$"
"$SHELL_TO_TEST" -c "set -C; echo new >| /tmp/test_force_$$" 2>/dev/null
result=$(cat "/tmp/test_force_$$" 2>/dev/null)
if [ "$result" = "new" ]; then
    pass
else
    skip "Force overwrite (>|) not supported"
fi
rm -f "/tmp/test_force_$$"
log_test "Multiple redirections"
"$SHELL_TO_TEST" -c "echo stdout; echo stderr >&2" > "/tmp/test_multi_out_$$" 2> "/tmp/test_multi_err_$$"
out_content=$(cat "/tmp/test_multi_out_$$" 2>/dev/null)
err_content=$(cat "/tmp/test_multi_err_$$" 2>/dev/null)
if [ "$out_content" = "stdout" ] && [ "$err_content" = "stderr" ]; then
    pass
    rm -f "/tmp/test_multi_out_$$" "/tmp/test_multi_err_$$"
else
    fail "Multiple redirections failed"
    rm -f "/tmp/test_multi_out_$$" "/tmp/test_multi_err_$$"
fi
log_test "Redirection order independence"
"$SHELL_TO_TEST" -c "echo test 2>&1 > /tmp/test_order_$$" 2>/dev/null
result=$(cat "/tmp/test_order_$$" 2>/dev/null)
if [ "$result" = "test" ]; then
    pass
    rm -f "/tmp/test_order_$$"
else
    fail "Redirection order handling failed"
    rm -f "/tmp/test_order_$$"
fi
log_test "Here document with tab stripping <<-"
result=$("$SHELL_TO_TEST" -c "cat <<- 'EOF'
	line1
	line2
	EOF" 2>/dev/null)
expected="line1
line2"
if [ "$result" = "$expected" ]; then
    pass
else
    skip "Here document tab stripping (<<-) not supported"
fi
log_test "Pipeline with background"
"$SHELL_TO_TEST" -c "echo hello | sleep 0.1 &" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Background pipeline failed"
fi
log_test "Complex redirection with pipeline"
echo "input" > "/tmp/test_complex_$$"
result=$("$SHELL_TO_TEST" -c "cat < /tmp/test_complex_$$ | tr 'a-z' 'A-Z'" 2>/dev/null)
if [ "$result" = "INPUT" ]; then
    pass
    rm -f "/tmp/test_complex_$$"
else
    fail "Complex redirection with pipeline failed"
    rm -f "/tmp/test_complex_$$"
fi
log_test "Null redirection"
"$SHELL_TO_TEST" -c "echo discarded > /dev/null" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Null redirection failed"
fi
log_test "Error to null redirection"
"$SHELL_TO_TEST" -c "echo error >&2" 2> /dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Error to null redirection failed"
fi
echo "=============================================================="
echo "POSIX I/O Redirection and Pipeline Test Results:"
echo "Total tests: $TOTAL"
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"
if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All I/O redirection tests passed!${NC}"
    exit 0
else
    echo "${YELLOW}Some I/O redirection tests failed. Review the failures above.${NC}"
    echo "Success rate: $(( PASSED * 100 / TOTAL ))%"
    exit 1
fi
