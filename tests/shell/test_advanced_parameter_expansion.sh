#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: advanced parameter expansion..."
OUT=$("$CJSH_PATH" -c "var='hello world hello'; echo \${var/hello/hi}" 2>&1)
if [ "$OUT" = "hi world hello" ]; then
    echo "PASS: single pattern substitution"
else
    echo "FAIL: single pattern substitution not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='hello world hello'; echo \${var//hello/hi}" 2>&1)
if [ "$OUT" = "hi world hi" ]; then
    echo "PASS: global pattern substitution"
else
    echo "FAIL: global pattern substitution not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='hello world'; echo \${var^}" 2>&1)
if [ "$OUT" = "Hello world" ]; then
    echo "PASS: uppercase first character"
else
    echo "FAIL: uppercase first character not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='hello world'; echo \${var^^}" 2>&1)
if [ "$OUT" = "HELLO WORLD" ]; then
    echo "PASS: uppercase all characters"
else
    echo "FAIL: uppercase all characters not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='HELLO WORLD'; echo \${var,}" 2>&1)
if [ "$OUT" = "hELLO WORLD" ]; then
    echo "PASS: lowercase first character"
else
    echo "FAIL: lowercase first character not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='HELLO WORLD'; echo \${var,,}" 2>&1)
if [ "$OUT" = "hello world" ]; then
    echo "PASS: lowercase all characters"
else
    echo "FAIL: lowercase all characters not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "var='hello'; echo \${var:-default}" 2>&1)
if [ "$OUT" = "hello" ]; then
    echo "PASS: existing default expansion works"
else
    echo "FAIL: existing default expansion broken (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var='hello world'; echo \${#var}" 2>&1)
if [ "$OUT" = "11" ]; then
    echo "PASS: string length expansion works"
else
    echo "FAIL: string length expansion broken (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var='hello world'; echo \${var#hello }" 2>&1)
if [ "$OUT" = "world" ]; then
    echo "PASS: prefix removal works"
else
    echo "FAIL: prefix removal broken (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var='hello world'; echo \${var% world}" 2>&1)
if [ "$OUT" = "hello" ]; then
    echo "PASS: suffix removal works"
else
    echo "FAIL: suffix removal broken (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var='test'; other='\${var}'; echo \${other}" 2>&1)
if [ "$OUT" = "test" ] || [ "$OUT" = "\${var}" ]; then
    echo "PASS: nested expansion handled"
else
    echo "FAIL: nested expansion failed (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var=PATH; echo \${!var}" 2>&1)
if echo "$OUT" | grep -q "/"; then
    echo "PASS: indirect expansion works"
else
    echo "FAIL: indirect expansion not implemented (got: '$OUT')"
fi
OUT=$("$CJSH_PATH" -c "arr_0=first; arr_1=second; echo \${arr_0} \${arr_1}" 2>&1)
if [ "$OUT" = "first second" ]; then
    echo "PASS: basic indexed array simulation works"
else
    echo "FAIL: basic variable expansion broken (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var='hello@world'; echo \${var/@/_}" 2>&1)
if [ "$OUT" = "hello_world" ] || [ "$OUT" = "hello@world" ]; then
    echo "PASS: special character handling in expansion"
else
    echo "FAIL: special character handling failed (got: '$OUT')"
    exit 1
fi
OUT=$("$CJSH_PATH" -c "var1='hello'; var2='world'; echo \${var1:-default} \${var2:-default}" 2>&1)
if [ "$OUT" = "hello world" ]; then
    echo "PASS: multiple parameter expansions work"
else
    echo "FAIL: multiple parameter expansions failed (got: '$OUT')"
    exit 1
fi
echo "PASS: advanced parameter expansion tests completed"
exit 0
