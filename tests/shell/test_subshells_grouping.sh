#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: subshells and command grouping..."

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

OUT=$("$CJSH_PATH" -c "(echo hello)")
if [ "$OUT" = "hello" ]; then
    pass_test "basic subshell execution"
else
    fail_test "basic subshell (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "VAR=outer; (VAR=inner; echo \$VAR); echo \$VAR")
EXPECTED="inner
outer"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "subshell variable isolation"
else
    fail_test "subshell variable isolation (got '$OUT')"
fi

"$CJSH_PATH" -c "(exit 42); echo \$?" > /tmp/subshell_exit
OUT=$(cat /tmp/subshell_exit)
rm -f /tmp/subshell_exit
if [ "$OUT" = "42" ]; then
    pass_test "subshell exit status propagates"
else
    fail_test "subshell exit status (got '$OUT', expected 42)"
fi

OUT=$("$CJSH_PATH" -c "VAR=a; (VAR=b; (VAR=c; echo \$VAR); echo \$VAR); echo \$VAR")
EXPECTED="c
b
a"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "nested subshells"
else
    fail_test "nested subshells (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "pwd > /tmp/pwd1; (cd /tmp); pwd > /tmp/pwd2; diff /tmp/pwd1 /tmp/pwd2")
rm -f /tmp/pwd1 /tmp/pwd2
if [ $? -eq 0 ]; then
    pass_test "subshell cd doesn't affect parent"
else
    fail_test "subshell cd isolation failed"
fi

OUT=$("$CJSH_PATH" -c "{ echo hello; echo world; }")
EXPECTED="hello
world"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "command grouping with braces"
else
    fail_test "brace grouping (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "VAR=outer; { VAR=inner; echo \$VAR; }; echo \$VAR")
EXPECTED="inner
inner"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "braces preserve shell state (variable changes persist)"
else
    fail_test "brace state preservation (got '$OUT')"
fi

cat > /tmp/test_grouping.sh << 'EOF'
#!/bin/sh
VAR=original
(VAR=subshell)
echo "after subshell: $VAR"
{ VAR=braces; }
echo "after braces: $VAR"
EOF
chmod +x /tmp/test_grouping.sh

OUT=$("$CJSH_PATH" /tmp/test_grouping.sh)
EXPECTED="after subshell: original
after braces: braces"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "braces vs subshell variable behavior"
else
    fail_test "braces vs subshell (got '$OUT')"
fi
rm -f /tmp/test_grouping.sh

OUT=$("$CJSH_PATH" -c "(echo one; echo two; echo three) | grep two")
if [ "$OUT" = "two" ]; then
    pass_test "subshell with pipeline"
else
    fail_test "subshell pipeline (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "{ echo line1; echo line2; } > /tmp/brace_redir; cat /tmp/brace_redir")
rm -f /tmp/brace_redir
EXPECTED="line1
line2"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "brace group with redirection"
else
    fail_test "brace redirection (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "(echo sub1; echo sub2) > /tmp/sub_redir; cat /tmp/sub_redir")
rm -f /tmp/sub_redir
EXPECTED="sub1
sub2"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "subshell with redirection"
else
    fail_test "subshell redirection (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "export TESTVAR=value; (echo \$TESTVAR)")
if [ "$OUT" = "value" ]; then
    pass_test "subshell inherits environment"
else
    fail_test "subshell environment (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "RESULT=\$(( echo nested; )); echo \$RESULT")
if [ "$OUT" = "nested" ]; then
    pass_test "subshell in command substitution"
else
    fail_test "subshell command substitution (got '$OUT')"
fi

"$CJSH_PATH" -c "{ false; }; echo \$?" > /tmp/brace_status
OUT=$(cat /tmp/brace_status)
rm -f /tmp/brace_status
if [ "$OUT" = "1" ]; then
    pass_test "brace group return status"
else
    fail_test "brace return status (got '$OUT', expected 1)"
fi

OUT=$("$CJSH_PATH" -c "(echo a; echo b; echo c)")
EXPECTED="a
b
c"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "multiple commands in subshell with semicolons"
else
    fail_test "subshell semicolons (got '$OUT')"
fi

"$CJSH_PATH" -c "(sleep 0.1 &); wait" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass_test "subshell with background process"
else
    fail_test "subshell background process"
fi

OUT=$("$CJSH_PATH" -c "()")
if [ -z "$OUT" ]; then
    pass_test "empty subshell"
else
    fail_test "empty subshell should produce no output (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "{ :; }")
if [ $? -eq 0 ]; then
    pass_test "empty brace group"
else
    fail_test "empty brace group failed"
fi

OUT=$("$CJSH_PATH" -c "(for i in 1 2 3; do echo \$i; done)")
EXPECTED="1
2
3"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "subshell with loop"
else
    fail_test "subshell loop (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "{ if true; then echo yes; else echo no; fi; }")
if [ "$OUT" = "yes" ]; then
    pass_test "brace group with conditional"
else
    fail_test "brace conditional (got '$OUT')"
fi

cat > /tmp/test_umask_subshell.sh << 'EOF'
#!/bin/sh
umask 0022
BEFORE=$(umask)
(umask 0077)
AFTER=$(umask)
if [ "$BEFORE" = "$AFTER" ]; then
    echo "isolated"
else
    echo "not isolated"
fi
EOF
chmod +x /tmp/test_umask_subshell.sh

OUT=$("$CJSH_PATH" /tmp/test_umask_subshell.sh)
if [ "$OUT" = "isolated" ]; then
    pass_test "subshell umask isolation"
else
    fail_test "subshell umask not isolated"
fi
rm -f /tmp/test_umask_subshell.sh

cat > /tmp/test_cd_braces.sh << 'EOF'
#!/bin/sh
cd /tmp
BEFORE=$(pwd)
{ cd /; }
AFTER=$(pwd)
if [ "$BEFORE" != "$AFTER" ]; then
    echo "changed"
else
    echo "unchanged"
fi
EOF
chmod +x /tmp/test_cd_braces.sh

OUT=$("$CJSH_PATH" /tmp/test_cd_braces.sh)
if [ "$OUT" = "changed" ]; then
    pass_test "brace group cd affects parent"
else
    fail_test "brace cd should affect parent"
fi
rm -f /tmp/test_cd_braces.sh

echo ""
echo "Subshells and Command Grouping Tests Summary:"
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
