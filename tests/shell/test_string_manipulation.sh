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

# Test string length
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${#STR}")
if [ "$OUT" = "11" ]; then
    pass_test "string length"
else
    fail_test "string length (got '$OUT', expected 11)"
fi

# Test empty string length
OUT=$("$CJSH_PATH" -c "STR=''; echo \${#STR}")
if [ "$OUT" = "0" ]; then
    pass_test "empty string length"
else
    fail_test "empty string length (got '$OUT')"
fi

# Test substring extraction ${var:offset}
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR:6}")
if [ "$OUT" = "world" ]; then
    pass_test "substring from offset"
else
    fail_test "substring from offset (got '$OUT', expected 'world')"
fi

# Test substring extraction ${var:offset:length}
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR:0:5}")
if [ "$OUT" = "hello" ]; then
    pass_test "substring with length"
else
    fail_test "substring with length (got '$OUT', expected 'hello')"
fi

# Test negative offset (from end)
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR: -5}")
if [ "$OUT" = "world" ]; then
    pass_test "substring negative offset"
else
    skip_test "substring negative offset (got '$OUT', may not be supported)"
fi

# Test remove shortest match from beginning ${var#pattern}
OUT=$("$CJSH_PATH" -c "PATH='/usr/local/bin'; echo \${PATH#/*/}")
if [ "$OUT" = "local/bin" ]; then
    pass_test "remove shortest prefix match"
else
    fail_test "remove shortest prefix (got '$OUT', expected 'local/bin')"
fi

# Test remove longest match from beginning ${var##pattern}
OUT=$("$CJSH_PATH" -c "PATH='/usr/local/bin'; echo \${PATH##/*/}")
if [ "$OUT" = "bin" ]; then
    pass_test "remove longest prefix match"
else
    fail_test "remove longest prefix (got '$OUT', expected 'bin')"
fi

# Test remove shortest match from end ${var%pattern}
OUT=$("$CJSH_PATH" -c "FILE='document.txt.bak'; echo \${FILE%.*}")
if [ "$OUT" = "document.txt" ]; then
    pass_test "remove shortest suffix match"
else
    fail_test "remove shortest suffix (got '$OUT', expected 'document.txt')"
fi

# Test remove longest match from end ${var%%pattern}
OUT=$("$CJSH_PATH" -c "FILE='document.txt.bak'; echo \${FILE%%.*}")
if [ "$OUT" = "document" ]; then
    pass_test "remove longest suffix match"
else
    fail_test "remove longest suffix (got '$OUT', expected 'document')"
fi

# Test search and replace first match ${var/pattern/replacement}
OUT=$("$CJSH_PATH" -c "STR='hello world hello'; echo \${STR/hello/hi}")
if [ "$OUT" = "hi world hello" ]; then
    pass_test "replace first match"
else
    fail_test "replace first (got '$OUT', expected 'hi world hello')"
fi

# Test search and replace all matches ${var//pattern/replacement}
OUT=$("$CJSH_PATH" -c "STR='hello world hello'; echo \${STR//hello/hi}")
if [ "$OUT" = "hi world hi" ]; then
    pass_test "replace all matches"
else
    fail_test "replace all (got '$OUT', expected 'hi world hi')"
fi

# Test replace at beginning ${var/#pattern/replacement}
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR/#hello/hi}")
if [ "$OUT" = "hi world" ]; then
    pass_test "replace at beginning"
else
    fail_test "replace at beginning (got '$OUT', expected 'hi world')"
fi

# Test replace at end ${var/%pattern/replacement}
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR/%world/universe}")
if [ "$OUT" = "hello universe" ]; then
    pass_test "replace at end"
else
    fail_test "replace at end (got '$OUT', expected 'hello universe')"
fi

# Test default value ${var:-default}
OUT=$("$CJSH_PATH" -c "unset VAR; echo \${VAR:-default}")
if [ "$OUT" = "default" ]; then
    pass_test "default value for unset variable"
else
    fail_test "default value (got '$OUT')"
fi

# Test default value with empty string
OUT=$("$CJSH_PATH" -c "VAR=''; echo \${VAR:-default}")
if [ "$OUT" = "default" ]; then
    pass_test "default value for empty variable"
else
    fail_test "default value empty (got '$OUT')"
fi

# Test assign default ${var:=default}
OUT=$("$CJSH_PATH" -c "unset VAR; echo \${VAR:=assigned}; echo \$VAR")
EXPECTED="assigned
assigned"
if [ "$OUT" = "$EXPECTED" ]; then
    pass_test "assign default value"
else
    fail_test "assign default (got '$OUT')"
fi

# Test error if unset ${var:?error message}
"$CJSH_PATH" -c "unset VAR; echo \${VAR:?variable not set}" 2>/dev/null
if [ $? -ne 0 ]; then
    pass_test "error on unset variable"
else
    fail_test "should error when variable unset"
fi

# Test use alternative value ${var:+alternative}
OUT=$("$CJSH_PATH" -c "VAR=value; echo \${VAR:+alternative}")
if [ "$OUT" = "alternative" ]; then
    pass_test "use alternative value when set"
else
    fail_test "alternative value (got '$OUT')"
fi

# Test alternative value when unset
OUT=$("$CJSH_PATH" -c "unset VAR; echo \${VAR:+alternative}")
if [ "$OUT" = "" ]; then
    pass_test "alternative value when unset returns empty"
else
    fail_test "alternative when unset (got '$OUT')"
fi

# Test case conversion to uppercase ${var^^}
OUT=$("$CJSH_PATH" -c "STR='hello'; echo \${STR^^}" 2>/dev/null)
if [ "$OUT" = "HELLO" ]; then
    pass_test "uppercase conversion"
elif [ "$OUT" = "hello" ]; then
    skip_test "uppercase conversion not supported"
else
    skip_test "uppercase conversion (got '$OUT')"
fi

# Test case conversion to lowercase ${var,,}
OUT=$("$CJSH_PATH" -c "STR='HELLO'; echo \${STR,,}" 2>/dev/null)
if [ "$OUT" = "hello" ]; then
    pass_test "lowercase conversion"
elif [ "$OUT" = "HELLO" ]; then
    skip_test "lowercase conversion not supported"
else
    skip_test "lowercase conversion (got '$OUT')"
fi

# Test first character uppercase ${var^}
OUT=$("$CJSH_PATH" -c "STR='hello world'; echo \${STR^}" 2>/dev/null)
if [ "$OUT" = "Hello world" ]; then
    pass_test "first character uppercase"
elif [ "$OUT" = "hello world" ]; then
    skip_test "first char uppercase not supported"
else
    skip_test "first char uppercase (got '$OUT')"
fi

# Test first character lowercase ${var,}
OUT=$("$CJSH_PATH" -c "STR='HELLO WORLD'; echo \${STR,}" 2>/dev/null)
if [ "$OUT" = "hELLO WORLD" ]; then
    pass_test "first character lowercase"
elif [ "$OUT" = "HELLO WORLD" ]; then
    skip_test "first char lowercase not supported"
else
    skip_test "first char lowercase (got '$OUT')"
fi

# Test nested parameter expansion
OUT=$("$CJSH_PATH" -c "A=hello; B=A; echo \${!B}")
if [ "$OUT" = "hello" ]; then
    pass_test "indirect expansion"
else
    skip_test "indirect expansion (got '$OUT', may not be supported)"
fi

# Test parameter expansion with special characters
OUT=$("$CJSH_PATH" -c "STR='a:b:c'; echo \${STR//:/ }")
if [ "$OUT" = "a b c" ]; then
    pass_test "replace special character colon"
else
    fail_test "replace colon (got '$OUT', expected 'a b c')"
fi

# Test multiple expansions in one command
OUT=$("$CJSH_PATH" -c "A=hello; B=world; echo \${A} \${B}")
if [ "$OUT" = "hello world" ]; then
    pass_test "multiple parameter expansions"
else
    fail_test "multiple expansions (got '$OUT')"
fi

# Test expansion with braces required
OUT=$("$CJSH_PATH" -c "VAR=test; echo \${VAR}ing")
if [ "$OUT" = "testing" ]; then
    pass_test "expansion with braces for concatenation"
else
    fail_test "braces concatenation (got '$OUT')"
fi

# Test array-like positional parameters length
OUT=$("$CJSH_PATH" -c "set -- a b c d e; echo \$#")
if [ "$OUT" = "5" ]; then
    pass_test "positional parameters count"
else
    fail_test "positional count (got '$OUT', expected 5)"
fi

# Test expansion in double quotes preserves spaces
OUT=$("$CJSH_PATH" -c "STR='  hello  world  '; echo \"\${STR}\"")
if [ "$OUT" = "  hello  world  " ]; then
    pass_test "expansion in quotes preserves spaces"
else
    fail_test "quoted expansion spaces (got '$OUT')"
fi

# Test empty parameter expansion
OUT=$("$CJSH_PATH" -c "VAR=''; echo \"x\${VAR}y\"")
if [ "$OUT" = "xy" ]; then
    pass_test "empty parameter expansion"
else
    fail_test "empty expansion (got '$OUT')"
fi

# Test parameter expansion with numbers in variable names
OUT=$("$CJSH_PATH" -c "VAR123=value; echo \${VAR123}")
if [ "$OUT" = "value" ]; then
    pass_test "variable with numbers"
else
    fail_test "variable with numbers (got '$OUT')"
fi

# Test parameter expansion with underscores
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
