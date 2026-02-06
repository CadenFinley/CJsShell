#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: advanced redirection patterns..."

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

"$CJSH_PATH" -c "echo stdout; echo stderr >&2" > /tmp/both_redirect 2>&1
OUT=$(cat /tmp/both_redirect)
rm -f /tmp/both_redirect
if echo "$OUT" | grep -q "stdout" && echo "$OUT" | grep -q "stderr"; then
    pass_test "redirect both stdout and stderr with &>"
else
    fail_test "both redirect &> (got '$OUT')"
fi

"$CJSH_PATH" -c "echo stdout; echo stderr >&2" > /tmp/both_redirect2 2>&1
OUT=$(cat /tmp/both_redirect2)
rm -f /tmp/both_redirect2
if echo "$OUT" | grep -q "stdout" && echo "$OUT" | grep -q "stderr"; then
    pass_test "redirect both with >file 2>&1"
else
    fail_test "both redirect >file 2>&1 (got '$OUT')"
fi

"$CJSH_PATH" -c "echo stdout; echo stderr >&2" 2>&1 > /tmp/order_test1
if [ -f /tmp/order_test1 ]; then
    OUT=$(cat /tmp/order_test1)
    rm -f /tmp/order_test1
    if [ "$OUT" = "stdout" ]; then
        pass_test "redirection order 2>&1 >file"
    else
        fail_test "redirection order 2>&1 >file (got '$OUT')"
    fi
else
    fail_test "redirection order test file not created"
fi

"$CJSH_PATH" -c "echo first > /tmp/clobber_test; echo second >| /tmp/clobber_test; cat /tmp/clobber_test"
OUT=$("$CJSH_PATH" -c "cat /tmp/clobber_test" 2>/dev/null)
rm -f /tmp/clobber_test
if [ "$OUT" = "second" ]; then
    pass_test "force overwrite with >|"
else
    fail_test "force overwrite >| (got '$OUT', expected 'second')"
fi

cat > /tmp/fd_test.txt << 'EOF'
line1
line2
line3
EOF

OUT=$("$CJSH_PATH" -c "exec 3< /tmp/fd_test.txt; read line <&3; echo \$line; exec 3<&-")
rm -f /tmp/fd_test.txt
if [ "$OUT" = "line1" ]; then
    pass_test "read from custom file descriptor"
else
    fail_test "custom fd read (got '$OUT')"
fi

"$CJSH_PATH" -c "exec 3> /tmp/fd_write_test; echo hello >&3; exec 3>&-"
if [ -f /tmp/fd_write_test ]; then
    OUT=$(cat /tmp/fd_write_test)
    rm -f /tmp/fd_write_test
    if [ "$OUT" = "hello" ]; then
        pass_test "write to custom file descriptor"
    else
        fail_test "custom fd write (got '$OUT')"
    fi
else
    fail_test "custom fd write file not created"
fi

"$CJSH_PATH" -c "exec 3> /tmp/fd_close_test; echo test >&3; exec 3>&-; echo fail >&3" 2>/dev/null
if [ $? -ne 0 ]; then
    rm -f /tmp/fd_close_test
    pass_test "closing file descriptor"
else
    rm -f /tmp/fd_close_test
    fail_test "closing file descriptor should error when writing to closed fd"
fi

"$CJSH_PATH" -c "exec 3>&1; echo to_fd3 >&3" > /tmp/dup_fd_test
OUT=$(cat /tmp/dup_fd_test)
rm -f /tmp/dup_fd_test
if [ "$OUT" = "to_fd3" ]; then
    pass_test "duplicating file descriptor"
else
    fail_test "fd duplication (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "cat <<< 'hello world'" 2>/dev/null)
if [ "$OUT" = "hello world" ]; then
    pass_test "here-string <<<"
else
    fail_test "here-string not supported (got '$OUT', expected 'hello world')"
fi

echo "first" > /tmp/append_test
"$CJSH_PATH" -c "echo second >> /tmp/append_test"
OUT=$(cat /tmp/append_test)
rm -f /tmp/append_test
EXPECTED="first
second"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "append with >>"
else
    fail_test "append mode (got '$OUT')"
fi

"$CJSH_PATH" -c "echo stdout; echo stderr >&2" 2> /tmp/stderr_only > /tmp/stdout_only
STDOUT=$(cat /tmp/stdout_only)
STDERR=$(cat /tmp/stderr_only)
rm -f /tmp/stderr_only /tmp/stdout_only
if [ "$STDOUT" = "stdout" ] && [ "$STDERR" = "stderr" ]; then
    pass_test "separate stdout and stderr redirection"
else
    fail_test "separate redirection (stdout='$STDOUT', stderr='$STDERR')"
fi

"$CJSH_PATH" -c "echo stdout; echo stderr >&2" 3>&1 1>&2 2>&3 > /tmp/swapped 2>&1
OUT=$(cat /tmp/swapped)
rm -f /tmp/swapped
if echo "$OUT" | grep -q "stdout" && echo "$OUT" | grep -q "stderr"; then
    pass_test "swapping stdout and stderr"
else
    fail_test "swapping stdout/stderr failed (got '$OUT')"
fi

echo "input_data" > /tmp/input_test
OUT=$("$CJSH_PATH" -c "cat < /tmp/input_test")
rm -f /tmp/input_test
if [ "$OUT" = "input_data" ]; then
    pass_test "input redirection with <"
else
    fail_test "input redirection (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "echo \$(echo inner > /tmp/inner_redir; cat /tmp/inner_redir)")
rm -f /tmp/inner_redir
if [ "$OUT" = "inner" ]; then
    pass_test "redirection within command substitution"
else
    fail_test "command substitution redirection (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "echo hidden > /dev/null; echo visible")
if [ "$OUT" = "visible" ]; then
    pass_test "/dev/null redirection"
else
    fail_test "/dev/null (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "echo visible; echo hidden >&2" 2>/dev/null)
if [ "$OUT" = "visible" ]; then
    pass_test "stderr to /dev/null"
else
    fail_test "stderr to /dev/null (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "if true; then echo yes; fi > /tmp/if_redir; cat /tmp/if_redir")
rm -f /tmp/if_redir
if [ "$OUT" = "yes" ]; then
    pass_test "redirection on if statement"
else
    fail_test "if redirection (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "for i in 1 2 3; do echo \$i; done > /tmp/loop_redir; cat /tmp/loop_redir")
rm -f /tmp/loop_redir
EXPECTED="1
2
3"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "redirection on for loop"
else
    fail_test "loop redirection (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "i=1; while [ \$i -le 3 ]; do echo \$i; i=\$((i+1)); done > /tmp/while_redir; cat /tmp/while_redir")
rm -f /tmp/while_redir
EXPECTED="1
2
3"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "redirection on while loop"
else
    fail_test "while redirection (got '$OUT')"
fi

"$CJSH_PATH" -c "echo test" > /tmp/multi1 > /tmp/multi2
if [ -f /tmp/multi2 ]; then
    OUT=$(cat /tmp/multi2)
    rm -f /tmp/multi1 /tmp/multi2
    if [ "$OUT" = "test" ]; then
        pass_test "multiple output redirections (last wins)"
    else
        fail_test "multiple redirections (got '$OUT')"
    fi
else
    rm -f /tmp/multi1 /tmp/multi2
    fail_test "multiple redirections file not created"
fi

OUT=$("$CJSH_PATH" -c "echo test > ~/test_tilde_redir_cjsh; cat ~/test_tilde_redir_cjsh; rm -f ~/test_tilde_redir_cjsh")
if [ "$OUT" = "test" ]; then
    pass_test "redirection with tilde expansion"
else
    fail_test "tilde expansion in redirection (got '$OUT')"
fi

"$CJSH_PATH" -c "FILE=/tmp/var_redir; echo test > \$FILE" < /dev/null > /tmp/var_expand_out 2>&1
if [ -f /tmp/var_redir ]; then
    OUT=$(cat /tmp/var_redir)
    rm -f /tmp/var_expand_out /tmp/var_redir
    if [ "$OUT" = "test" ]; then
        pass_test "redirection with variable expansion"
    else
        fail_test "variable expansion in redirection (got '$OUT', expected 'test')"
    fi
else
    rm -f /tmp/var_expand_out /tmp/var_redir
    fail_test "variable expansion in redirection (file not created)"
fi

echo ""
echo "Advanced Redirection Patterns Tests Summary:"
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
