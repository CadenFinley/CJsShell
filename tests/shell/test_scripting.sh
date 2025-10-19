#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: scripting and control flow..."
OUT=$("$CJSH_PATH" -c "if true; then echo 'true branch'; else echo 'false branch'; fi")
if [ "$OUT" != "true branch" ]; then
    echo "FAIL: if-then-else true case (got '$OUT')"
    exit 1
else
    echo "PASS: if-then-else true case"
fi
OUT=$("$CJSH_PATH" -c "if false; then echo 'true branch'; else echo 'false branch'; fi")
if [ "$OUT" != "false branch" ]; then
    echo "FAIL: if-then-else false case (got '$OUT')"
    exit 1
else
    echo "PASS: if-then-else false case"
fi
OUT=$("$CJSH_PATH" -c "if true; then echo 'success'; fi")
if [ "$OUT" != "success" ]; then
    echo "FAIL: if without else (got '$OUT')"
    exit 1
else
    echo "PASS: if without else"
fi
OUT=$("$CJSH_PATH" -c "if false; then echo 'first'; elif true; then echo 'second'; else echo 'third'; fi")
if [ "$OUT" != "second" ] && [ "$OUT" != "third" ]; then
    echo "FAIL: elif statement (got '$OUT')"
    exit 1
else
    echo "PASS: elif statement"
fi
OUT=$("$CJSH_PATH" -c "for i in 1 2 3; do echo \$i; done")
EXPECTED="1
2
3"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: for loop (got '$OUT')"
    exit 1
else
    echo "PASS: for loop"
fi
OUT=$("$CJSH_PATH" -c "i=1; while [ \$i -le 3 ]; do echo \$i; i=\$((i + 1)); done")
EXPECTED="1
2
3"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: while loop (got '$OUT')"
    exit 1
else
    echo "PASS: while loop"
fi
OUT=$("$CJSH_PATH" -c "echo \$((2 + 3))")
if [ "$OUT" != "5" ]; then
    echo "FAIL: arithmetic expansion addition (got '$OUT')"
    exit 1
else
    echo "PASS: arithmetic expansion addition"
fi
OUT=$("$CJSH_PATH" -c "echo \$((10 - 4))")
if [ "$OUT" != "6" ]; then
    echo "FAIL: arithmetic expansion subtraction (got '$OUT')"
    exit 1
else
    echo "PASS: arithmetic expansion subtraction"
fi
OUT=$("$CJSH_PATH" -c "echo \$((3 * 4))")
if [ "$OUT" != "12" ]; then
    echo "FAIL: arithmetic expansion multiplication (got '$OUT')"
    exit 1
else
    echo "PASS: arithmetic expansion multiplication"
fi
OUT=$("$CJSH_PATH" -c "true && echo 'and success'")
if [ "$OUT" != "and success" ]; then
    echo "FAIL: logical AND true case (got '$OUT')"
    exit 1
else
    echo "PASS: logical AND true case"
fi
OUT=$("$CJSH_PATH" -c "false && echo 'should not print'")
if [ -n "$OUT" ]; then
    echo "FAIL: logical AND false case (got '$OUT')"
    exit 1
else
    echo "PASS: logical AND false case"
fi
OUT=$("$CJSH_PATH" -c "false || echo 'or success'")
if [ "$OUT" != "or success" ]; then
    echo "FAIL: logical OR false case (got '$OUT')"
    exit 1
else
    echo "PASS: logical OR false case"
fi
OUT=$("$CJSH_PATH" -c "(echo first; echo second)")
EXPECTED="first
second"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: command grouping with parentheses (got '$OUT')"
    exit 1
else
    echo "PASS: command grouping with parentheses"
fi
OUT=$("$CJSH_PATH" -c "{ echo first; echo second; }")
EXPECTED="first
second"
if [ "$OUT" != "$EXPECTED" ]; then
    echo "FAIL: command grouping with braces (got '$OUT')"
    exit 1
else
    echo "PASS: command grouping with braces"
fi
OUT=$("$CJSH_PATH" -c "case hello in hello) echo matched;; *) echo no match;; esac")
if [ "$OUT" != "matched" ]; then
    echo "FAIL: case statement match (got '$OUT')"
    exit 1
else
    echo "PASS: case statement match"
fi
OUT=$("$CJSH_PATH" -c "case xyz in hello) echo matched;; *) echo no match;; esac")
if [ "$OUT" != "no match" ]; then
    echo "FAIL: case statement no match (got '$OUT')"
    exit 1
else
    echo "PASS: case statement no match"
fi
OUT=$("$CJSH_PATH" -c "myfunc() { echo 'function called'; }; myfunc")
if [ "$OUT" != "function called" ]; then
    echo "FAIL: function definition and calling (got '$OUT')"
    exit 1
else
    echo "PASS: function definition and calling"
fi
OUT=$("$CJSH_PATH" -c "greet() { echo \"Hello \$1\"; }; greet World")
if [ "$OUT" != "Hello World" ]; then
    echo "FAIL: function with parameters (got '$OUT')"
    exit 1
else
    echo "PASS: function with parameters"
fi
echo "PASS"
exit 0
