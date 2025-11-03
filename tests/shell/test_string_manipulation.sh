#!/usr/bin/env sh
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: string manipulation and advanced parameter expansion..."

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

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${#STR}")
if [ "$OUT" = "11" ]; then
    pass_test "string length"
else
    fail_test "string length (got '$OUT', expected 11)"
fi

OUT=$("$CJSH_PATH" -c "STR=''; echo \${#STR}")
if [ "$OUT" = "0" ]; then
    pass_test "empty string length"
else
    fail_test "empty string length (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR:6}")
if [ "$OUT" = "world" ]; then
    pass_test "substring from offset"
else
    fail_test "substring from offset (got '$OUT', expected 'world')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR:0:5}")
if [ "$OUT" = "hello" ]; then
    pass_test "substring with length"
else
    fail_test "substring with length (got '$OUT', expected 'hello')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR: -5}")
if [ "$OUT" = "world" ]; then
    pass_test "substring negative offset"
else
    fail_test "substring negative offset (got '$OUT', expected 'world')"
fi

OUT=$("$CJSH_PATH" -c "PATH='/usr/local/bin'; echo \${PATH#/*/}")
if [ "$OUT" = "local/bin" ]; then
    pass_test "remove shortest prefix match"
else
    fail_test "remove shortest prefix (got '$OUT', expected 'local/bin')"
fi

OUT=$("$CJSH_PATH" -c "PATH='/usr/local/bin'; echo \${PATH##/*/}")
if [ "$OUT" = "bin" ]; then
    pass_test "remove longest prefix match"
else
    fail_test "remove longest prefix (got '$OUT', expected 'bin')"
fi

OUT=$("$CJSH_PATH" -c "FILE='document.txt.bak'; echo \${FILE%.*}")
if [ "$OUT" = "document.txt" ]; then
    pass_test "remove shortest suffix match"
else
    fail_test "remove shortest suffix (got '$OUT', expected 'document.txt')"
fi

OUT=$("$CJSH_PATH" -c "FILE='document.txt.bak'; echo \${FILE%%.*}")
if [ "$OUT" = "document" ]; then
    pass_test "remove longest suffix match"
else
    fail_test "remove longest suffix (got '$OUT', expected 'document')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world hello'; echo \${STR/hello/hi}")
if [ "$OUT" = "hi world hello" ]; then
    pass_test "replace first match"
else
    fail_test "replace first (got '$OUT', expected 'hi world hello')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world hello'; echo \${STR//hello/hi}")
if [ "$OUT" = "hi world hi" ]; then
    pass_test "replace all matches"
else
    fail_test "replace all (got '$OUT', expected 'hi world hi')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR/#hello/hi}")
if [ "$OUT" = "hi world" ]; then
    pass_test "replace at beginning"
else
    fail_test "replace at beginning (got '$OUT', expected 'hi world')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR/%world/universe}")
if [ "$OUT" = "hello universe" ]; then
    pass_test "replace at end"
else
    fail_test "replace at end (got '$OUT', expected 'hello universe')"
fi

OUT=$("$CJSH_PATH" -c "unset VAR; echo \${VAR:-default}")
if [ "$OUT" = "default" ]; then
    pass_test "default value for unset variable"
else
    fail_test "default value (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "VAR=''; echo \${VAR:-default}")
if [ "$OUT" = "default" ]; then
    pass_test "default value for empty variable"
else
    fail_test "default value empty (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "unset VAR; echo \${VAR:=assigned}; echo \$VAR")
EXPECTED="assigned
assigned"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "assign default value"
else
    fail_test "assign default (got '$OUT')"
fi

"$CJSH_PATH" -c "unset VAR; echo \${VAR:?variable not set}" 2>/dev/null
if [ $? -ne 0 ]; then
    pass_test "error on unset variable"
else
    fail_test "should error when variable unset"
fi

OUT=$("$CJSH_PATH" -c "VAR=value; echo \${VAR:+alternative}")
if [ "$OUT" = "alternative" ]; then
    pass_test "use alternative value when set"
else
    fail_test "alternative value (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "unset VAR; echo \${VAR:+alternative}")
if [ "$OUT" = "" ]; then
    pass_test "alternative value when unset returns empty"
else
    fail_test "alternative when unset (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello'; echo \${STR^^}" 2>/dev/null)
if [ "$OUT" = "HELLO" ]; then
    pass_test "uppercase conversion"
elif [ "$OUT" = "hello" ]; then
    fail_test "uppercase conversion not supported"
else
    fail_test "uppercase conversion (got '$OUT', expected 'HELLO')"
fi

OUT=$("$CJSH_PATH" -c "STR='HELLO'; echo \${STR,,}" 2>/dev/null)
if [ "$OUT" = "hello" ]; then
    pass_test "lowercase conversion"
elif [ "$OUT" = "HELLO" ]; then
    fail_test "lowercase conversion not supported"
else
    fail_test "lowercase conversion (got '$OUT', expected 'hello')"
fi

OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR^}" 2>/dev/null)
if [ "$OUT" = "Hello world" ]; then
    pass_test "first character uppercase"
elif [ "$OUT" = "hello world" ]; then
    fail_test "first char uppercase not supported"
else
    fail_test "first char uppercase (got '$OUT', expected 'Hello world')"
fi

OUT=$("$CJSH_PATH" -c "STR='HELLO WORLD'; echo \${STR,}" 2>/dev/null)
if [ "$OUT" = "hELLO WORLD" ]; then
    pass_test "first character lowercase"
elif [ "$OUT" = "HELLO WORLD" ]; then
    fail_test "first char lowercase not supported"
else
    fail_test "first char lowercase (got '$OUT', expected 'hELLO WORLD')"
fi

OUT=$("$CJSH_PATH" -c "A=hello; B=A; echo \${!B}")
if [ "$OUT" = "hello" ]; then
    pass_test "indirect expansion"
else
    fail_test "indirect expansion (got '$OUT', expected 'hello')"
fi

OUT=$("$CJSH_PATH" -c "STR='a:b:c'; echo \${STR//:/ }")
if [ "$OUT" = "a b c" ]; then
    pass_test "replace special character colon"
else
    fail_test "replace colon (got '$OUT', expected 'a b c')"
fi

OUT=$("$CJSH_PATH" -c "A=hello; B=world; echo \${A} \${B}")
if [ "$OUT" = "hello world" ]; then
    pass_test "multiple parameter expansions"
else
    fail_test "multiple expansions (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "VAR=test; echo \${VAR}ing")
if [ "$OUT" = "testing" ]; then
    pass_test "expansion with braces for concatenation"
else
    fail_test "braces concatenation (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "set -- a b c d e; echo \$#")
if [ "$OUT" = "5" ]; then
    pass_test "positional parameters count"
else
    fail_test "positional count (got '$OUT', expected 5)"
fi

OUT=$("$CJSH_PATH" -c "STR='  hello  world  '; echo \"\${STR}\"")
if [ "$OUT" = "  hello  world  " ]; then
    pass_test "expansion in quotes preserves spaces"
else
    fail_test "quoted expansion spaces (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "VAR=''; echo \"x\${VAR}y\"")
if [ "$OUT" = "xy" ]; then
    pass_test "empty parameter expansion"
else
    fail_test "empty expansion (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "VAR123=value; echo \${VAR123}")
if [ "$OUT" = "value" ]; then
    pass_test "variable with numbers"
else
    fail_test "variable with numbers (got '$OUT')"
fi

OUT=$("$CJSH_PATH" -c "MY_VAR=value; echo \${MY_VAR}")
if [ "$OUT" = "value" ]; then
    pass_test "variable with underscores"
else
    fail_test "variable with underscores (got '$OUT')"
fi

echo ""
echo "String Manipulation Tests Summary:"
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
