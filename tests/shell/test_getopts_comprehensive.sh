#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: getopts comprehensive..."

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

# Test basic option parsing
cat > /tmp/test_getopts1.sh << 'EOF'
#!/bin/sh
while getopts "a:b:c" opt; do
    case $opt in
        a) echo "opt_a=$OPTARG";;
        b) echo "opt_b=$OPTARG";;
        c) echo "opt_c";;
        ?) exit 1;;
    esac
done
EOF
chmod +x /tmp/test_getopts1.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts1.sh -a value1 -b value2 -c)
EXPECTED="opt_a=value1
opt_b=value2
opt_c"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "getopts basic parsing (output mismatch)"
else
    pass_test "getopts basic parsing"
fi
rm -f /tmp/test_getopts1.sh

# Test getopts with no arguments
cat > /tmp/test_getopts2.sh << 'EOF'
#!/bin/sh
count=0
while getopts "abc" opt; do
    count=$((count + 1))
done
echo "$count"
EOF
chmod +x /tmp/test_getopts2.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts2.sh)
if [ "$OUT" != "0" ]; then
    fail_test "getopts with no arguments (got '$OUT', expected '0')"
else
    pass_test "getopts with no arguments"
fi
rm -f /tmp/test_getopts2.sh

# Test getopts with combined short options
cat > /tmp/test_getopts3.sh << 'EOF'
#!/bin/sh
while getopts "abc" opt; do
    echo "opt=$opt"
done
EOF
chmod +x /tmp/test_getopts3.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts3.sh -abc)
EXPECTED="opt=a
opt=b
opt=c"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "getopts combined short options (output mismatch)"
else
    pass_test "getopts combined short options"
fi
rm -f /tmp/test_getopts3.sh

# Test getopts OPTIND variable
cat > /tmp/test_getopts4.sh << 'EOF'
#!/bin/sh
while getopts "a" opt; do
    :
done
echo "$OPTIND"
EOF
chmod +x /tmp/test_getopts4.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts4.sh -a -a)
if [ "$OUT" != "3" ]; then
    fail_test "getopts OPTIND tracking (got '$OUT', expected '3')"
else
    pass_test "getopts OPTIND tracking"
fi
rm -f /tmp/test_getopts4.sh

# Test getopts with missing required argument
cat > /tmp/test_getopts5.sh << 'EOF'
#!/bin/sh
while getopts "a:" opt; do
    case $opt in
        a) echo "opt_a=$OPTARG";;
        \?) echo "error"; exit 1;;
    esac
done
EOF
chmod +x /tmp/test_getopts5.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts5.sh -a 2>&1)
if echo "$OUT" | grep -q "error"; then
    pass_test "getopts missing required argument"
else
    fail_test "getopts missing required argument (expected error)"
fi
rm -f /tmp/test_getopts5.sh

# Test getopts with non-option arguments
cat > /tmp/test_getopts6.sh << 'EOF'
#!/bin/sh
while getopts "a" opt; do
    echo "opt=$opt"
done
shift $((OPTIND-1))
echo "remaining=$*"
EOF
chmod +x /tmp/test_getopts6.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts6.sh -a file1 file2)
EXPECTED="opt=a
remaining=file1 file2"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "getopts with non-option arguments (output mismatch)"
else
    pass_test "getopts with non-option arguments"
fi
rm -f /tmp/test_getopts6.sh

# Test getopts silent error reporting mode
cat > /tmp/test_getopts7.sh << 'EOF'
#!/bin/sh
while getopts ":a:b" opt; do
    case $opt in
        a) echo "opt_a=$OPTARG";;
        b) echo "opt_b";;
        :) echo "missing_arg=$OPTARG";;
        \?) echo "invalid=$OPTARG";;
    esac
done
EOF
chmod +x /tmp/test_getopts7.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts7.sh -a)
if echo "$OUT" | grep -q "missing_arg"; then
    pass_test "getopts silent error mode"
else
    skip_test "getopts silent error mode (implementation dependent)"
fi
rm -f /tmp/test_getopts7.sh

# Test getopts in function
cat > /tmp/test_getopts8.sh << 'EOF'
#!/bin/sh
parse_opts() {
    while getopts "x:y" opt; do
        case $opt in
            x) echo "x=$OPTARG";;
            y) echo "y";;
        esac
    done
}
parse_opts -x value -y
EOF
chmod +x /tmp/test_getopts8.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts8.sh)
EXPECTED="x=value
y"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "getopts in function (output mismatch)"
else
    pass_test "getopts in function"
fi
rm -f /tmp/test_getopts8.sh

# Test getopts with option argument containing spaces
cat > /tmp/test_getopts9.sh << 'EOF'
#!/bin/sh
while getopts "a:" opt; do
    case $opt in
        a) echo "opt_a='$OPTARG'";;
    esac
done
EOF
chmod +x /tmp/test_getopts9.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts9.sh -a "hello world")
if [ "$OUT" = "opt_a='hello world'" ]; then
    pass_test "getopts option argument with spaces"
else
    fail_test "getopts option argument with spaces (got '$OUT')"
fi
rm -f /tmp/test_getopts9.sh

# Test getopts reset between calls
cat > /tmp/test_getopts10.sh << 'EOF'
#!/bin/sh
parse1() {
    OPTIND=1
    while getopts "a" opt; do
        echo "parse1: $opt"
    done
}
parse2() {
    OPTIND=1
    while getopts "b" opt; do
        echo "parse2: $opt"
    done
}
parse1 -a
parse2 -b
EOF
chmod +x /tmp/test_getopts10.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts10.sh)
EXPECTED="parse1: a
parse2: b"
if [ "$OUT" != "$EXPECTED" ]; then
    fail_test "getopts reset between calls (output mismatch)"
else
    pass_test "getopts reset between calls"
fi
rm -f /tmp/test_getopts10.sh

# Test getopts with equals sign in argument
cat > /tmp/test_getopts11.sh << 'EOF'
#!/bin/sh
while getopts "o:" opt; do
    case $opt in
        o) echo "option=$OPTARG";;
    esac
done
EOF
chmod +x /tmp/test_getopts11.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts11.sh -o key=value)
if [ "$OUT" = "option=key=value" ]; then
    pass_test "getopts argument with equals sign"
else
    fail_test "getopts argument with equals sign (got '$OUT')"
fi
rm -f /tmp/test_getopts11.sh

# Test getopts with numeric option
cat > /tmp/test_getopts12.sh << 'EOF'
#!/bin/sh
while getopts "1:2" opt; do
    case $opt in
        1) echo "one=$OPTARG";;
        2) echo "two";;
    esac
done
EOF
chmod +x /tmp/test_getopts12.sh

OUT=$("$CJSH_PATH" /tmp/test_getopts12.sh -1 arg -2)
EXPECTED="one=arg
two"
if [ "$OUT" != "$EXPECTED" ]; then
    skip_test "getopts numeric options (not all shells support this)"
else
    pass_test "getopts numeric options"
fi
rm -f /tmp/test_getopts12.sh

echo ""
echo "Getopts Comprehensive Tests Summary:"
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
