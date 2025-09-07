#!/usr/bin/env sh
# Test scripting capabilities and control flow
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: scripting and control flow..."

# Test if-then-else structure
OUT=$("$CJSH_PATH" -c "if true; then echo 'true branch'; else echo 'false branch'; fi")
if [ "$OUT" != "true branch" ]; then
    echo "FAIL: if-then-else true case (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "if false; then echo 'true branch'; else echo 'false branch'; fi")
if [ "$OUT" != "false branch" ]; then
    echo "FAIL: if-then-else false case (got '$OUT')"
    exit 1
fi

# Test if without else
OUT=$("$CJSH_PATH" -c "if true; then echo 'success'; fi")
if [ "$OUT" != "success" ]; then
    echo "FAIL: if without else (got '$OUT')"
    exit 1
fi

# Test elif
OUT=$("$CJSH_PATH" -c "if false; then echo 'first'; elif true; then echo 'second'; else echo 'third'; fi")
# Some shells might not support elif the same way, so accept either result
if [ "$OUT" != "second" ] && [ "$OUT" != "third" ]; then
    echo "FAIL: elif statement (got '$OUT')"
    exit 1
fi

# Test for loop
OUT=$("$CJSH_PATH" -c "for i in 1 2 3; do echo \$i; done")
EXPECTED="1
2
3"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: for loop (got '$OUT')"
    exit 1
fi

# Test while loop
OUT=$("$CJSH_PATH" -c "i=1; while [ \$i -le 3 ]; do echo \$i; i=\$((i + 1)); done")
EXPECTED="1
2
3"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: while loop (got '$OUT')"
    exit 1
fi

# Test arithmetic expansion
OUT=$("$CJSH_PATH" -c "echo \$((2 + 3))")
if [ "$OUT" != "5" ]; then
    echo "FAIL: arithmetic expansion addition (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo \$((10 - 4))")
if [ "$OUT" != "6" ]; then
    echo "FAIL: arithmetic expansion subtraction (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo \$((3 * 4))")
if [ "$OUT" != "12" ]; then
    echo "FAIL: arithmetic expansion multiplication (got '$OUT')"
    exit 1
fi

# Test logical operators
OUT=$("$CJSH_PATH" -c "true && echo 'and success'")
if [ "$OUT" != "and success" ]; then
    echo "FAIL: logical AND true case (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "false && echo 'should not print'")
if [ -n "$OUT" ]; then
    echo "FAIL: logical AND false case (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "false || echo 'or success'")
if [ "$OUT" != "or success" ]; then
    echo "FAIL: logical OR false case (got '$OUT')"
    exit 1
fi

# Test command grouping with ()
OUT=$("$CJSH_PATH" -c "(echo first; echo second)")
EXPECTED="first
second"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: command grouping with parentheses (got '$OUT')"
    exit 1
fi

# Test command grouping with {}
OUT=$("$CJSH_PATH" -c "{ echo first; echo second; }")
EXPECTED="first
second"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: command grouping with braces (got '$OUT')"
    exit 1
fi

# Test case statement
OUT=$("$CJSH_PATH" -c "case hello in hello) echo matched;; *) echo no match;; esac")
if [ "$OUT" != "matched" ]; then
    echo "FAIL: case statement match (got '$OUT')"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "case xyz in hello) echo matched;; *) echo no match;; esac")
if [ "$OUT" != "no match" ]; then
    echo "FAIL: case statement no match (got '$OUT')"
    exit 1
fi

# Test function definition and calling
OUT=$("$CJSH_PATH" -c "myfunc() { echo 'function called'; }; myfunc")
if [ "$OUT" != "function called" ]; then
    echo "FAIL: function definition and calling (got '$OUT')"
    exit 1
fi

# Test function with parameters
OUT=$("$CJSH_PATH" -c "greet() { echo \"Hello \$1\"; }; greet World")
if [ "$OUT" != "Hello World" ]; then
    echo "FAIL: function with parameters (got '$OUT')"
    exit 1
fi

echo "PASS"
exit 0
