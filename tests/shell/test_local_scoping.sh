#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: local variable scoping..."
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
cat > /tmp/test_local1.sh << 'EOF'
test_func() {
    local VAR=inside
    echo "$VAR"
}
VAR=outside
test_func
echo "$VAR"
EOF
chmod +x /tmp/test_local1.sh
OUT=$("$CJSH_PATH" /tmp/test_local1.sh)
EXPECTED="inside
outside"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local variable doesn't affect global scope"
else
    fail_test "local variable scoping (got '$OUT')"
fi
rm -f /tmp/test_local1.sh
cat > /tmp/test_local2.sh << 'EOF'
GLOBAL=global_value
test_func() {
    local GLOBAL=local_value
    echo "$GLOBAL"
}
test_func
echo "$GLOBAL"
EOF
chmod +x /tmp/test_local2.sh
OUT=$("$CJSH_PATH" /tmp/test_local2.sh)
EXPECTED="local_value
global_value"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local variable shadows global"
else
    fail_test "local variable shadowing (got '$OUT')"
fi
rm -f /tmp/test_local2.sh
cat > /tmp/test_local3.sh << 'EOF'
test_func() {
    local VAR1=one VAR2=two VAR3=three
    echo "$VAR1 $VAR2 $VAR3"
}
test_func
EOF
chmod +x /tmp/test_local3.sh
OUT=$("$CJSH_PATH" /tmp/test_local3.sh)
if [ "$OUT" = "one two three" ]; then
    pass_test "multiple local variables in one declaration"
else
    fail_test "multiple local variables (got '$OUT')"
fi
rm -f /tmp/test_local3.sh
cat > /tmp/test_local4.sh << 'EOF'
OUTER=outer_value
test_func() {
    local OUTER
    OUTER=inner_value
    echo "$OUTER"
}
test_func
echo "$OUTER"
EOF
chmod +x /tmp/test_local4.sh
OUT=$("$CJSH_PATH" /tmp/test_local4.sh)
EXPECTED="inner_value
outer_value"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local variable declaration without assignment"
else
    fail_test "local without assignment (got '$OUT')"
fi
rm -f /tmp/test_local4.sh
cat > /tmp/test_local5.sh << 'EOF'
outer_func() {
    local VAR=outer
    inner_func() {
        local VAR=inner
        echo "inner: $VAR"
    }
    inner_func
    echo "outer: $VAR"
}
VAR=global
outer_func
echo "global: $VAR"
EOF
chmod +x /tmp/test_local5.sh
OUT=$("$CJSH_PATH" /tmp/test_local5.sh)
EXPECTED="inner: inner
outer: outer
global: global"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local variables in nested functions"
else
    fail_test "nested function locals (got '$OUT')"
fi
rm -f /tmp/test_local5.sh
cat > /tmp/test_local6.sh << 'EOF'
test_func() {
    local COUNT=0
    COUNT=$((COUNT + 1))
    echo "$COUNT"
}
test_func
test_func
EOF
chmod +x /tmp/test_local6.sh
OUT=$("$CJSH_PATH" /tmp/test_local6.sh)
EXPECTED="1
1"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local variable resets between function calls"
else
    fail_test "local variable persistence (got '$OUT')"
fi
rm -f /tmp/test_local6.sh
OUT=$("$CJSH_PATH" -c "local VAR=value" 2>&1)
if [ $? -ne 0 ]; then
    pass_test "local outside function fails appropriately"
else
    fail_test "local should fail outside function"
fi
cat > /tmp/test_local7.sh << 'EOF'
test_func() {
    local VAR=local_val
    export VAR
    sh -c 'echo "$VAR"'
}
test_func
EOF
chmod +x /tmp/test_local7.sh
OUT=$("$CJSH_PATH" /tmp/test_local7.sh 2>&1)
if [ "$OUT" = "local_val" ]; then
    pass_test "local variable can be exported to subshells"
else
    fail_test "local with export (got '$OUT', expected 'local_val')"
fi
rm -f /tmp/test_local7.sh
cat > /tmp/test_local7b.sh << 'EOF'
test_func() {
    local VAR=local_val
    export VAR
}
VAR=global_val
test_func
echo "$VAR"
EOF
chmod +x /tmp/test_local7b.sh
OUT=$("$CJSH_PATH" /tmp/test_local7b.sh 2>&1)
if [ "$OUT" = "global_val" ]; then
    pass_test "exported local doesn't persist after function"
else
    fail_test "exported local persistence (got '$OUT', expected 'global_val')"
fi
rm -f /tmp/test_local7b.sh
cat > /tmp/test_local8.sh << 'EOF'
test_func() {
    local VAR='hello world $TEST'
    echo "$VAR"
}
test_func
EOF
chmod +x /tmp/test_local8.sh
OUT=$("$CJSH_PATH" /tmp/test_local8.sh)
if [ "$OUT" = 'hello world $TEST' ]; then
    pass_test "local variable with special characters"
else
    fail_test "local with special chars (got '$OUT')"
fi
rm -f /tmp/test_local8.sh
cat > /tmp/test_local9.sh << 'EOF'
factorial() {
    local n=$1
    if [ "$n" -le 1 ]; then
        echo 1
    else
        local prev=$(factorial $((n - 1)))
        echo $((n * prev))
    fi
}
factorial 5
EOF
chmod +x /tmp/test_local9.sh
OUT=$("$CJSH_PATH" /tmp/test_local9.sh)
if [ "$OUT" = "120" ]; then
    pass_test "local variables in recursive function"
else
    fail_test "recursive function locals (got '$OUT', expected 120)"
fi
rm -f /tmp/test_local9.sh
cat > /tmp/test_local10.sh << 'EOF'
readonly GLOBAL=readonly_val
test_func() {
    local GLOBAL=local_val
    echo "$GLOBAL"
}
test_func 2>&1
echo "$GLOBAL"
EOF
chmod +x /tmp/test_local10.sh
OUT=$("$CJSH_PATH" /tmp/test_local10.sh 2>&1)
EXPECTED="local_val
readonly_val"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local can shadow readonly variable"
else
    fail_test "local readonly shadowing (got '$OUT', expected '$EXPECTED')"
fi
rm -f /tmp/test_local10.sh
cat > /tmp/test_local11.sh << 'EOF'
test_func() {
    local VAR=value
    echo "before: $VAR"
    unset VAR
    echo "after: $VAR"
}
test_func
EOF
chmod +x /tmp/test_local11.sh
OUT=$("$CJSH_PATH" /tmp/test_local11.sh)
EXPECTED="before: value
after: "
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "local variable can be unset"
else
    fail_test "local unset (got '$OUT')"
fi
rm -f /tmp/test_local11.sh
cat > /tmp/test_local12.sh << 'EOF'
test_func() {
    local VAR=
    echo "empty: '$VAR'"
}
test_func
EOF
chmod +x /tmp/test_local12.sh
OUT=$("$CJSH_PATH" /tmp/test_local12.sh)
if [ "$OUT" = "empty: ''" ]; then
    pass_test "local variable with empty value"
else
    fail_test "local empty value (got '$OUT')"
fi
rm -f /tmp/test_local12.sh
echo ""
echo "Local Variable Scoping Tests Summary:"
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
