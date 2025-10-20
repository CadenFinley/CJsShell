#!/usr/bin/env sh
# Test: command substitution regression tests
# This test suite prevents regressions related to command substitution
# particularly when output contains shell syntax like variables, quotes, and special characters

if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi

echo "Test: command substitution regression tests..."

# Test 1: Variables in command substitution output should not be expanded
VAR_TEST=$("$CJSH_PATH" -c 'echo "$(printf '"'"'case $opt in'"'"')"' 2>&1)
if [ "$VAR_TEST" != "case \$opt in" ]; then
  echo "FAIL: variables in command substitution output were expanded (got: $VAR_TEST)"
  exit 1
else
  echo "PASS: variables in command substitution output preserved"
fi

# Test 2: Variables in command substitution should not expand even when defined in outer shell
VAR_TEST2=$("$CJSH_PATH" -c 'opt=TESTVALUE; echo "$(printf '"'"'case $opt in'"'"')"' 2>&1)
if [ "$VAR_TEST2" != "case \$opt in" ]; then
  echo "FAIL: variables in command substitution were incorrectly expanded (got: $VAR_TEST2)"
  exit 1
else
  echo "PASS: variables not expanded even when defined"
fi

# Test 3: Empty quoted strings in command substitution output should be preserved
EMPTY_QUOTES=$("$CJSH_PATH" -c 'echo "$(printf '"'"'test"" value'"'"')"' 2>&1)
if [ "$EMPTY_QUOTES" != "test\"\" value" ]; then
  echo "FAIL: empty quoted strings were removed (got: $EMPTY_QUOTES)"
  exit 1
else
  echo "PASS: empty quoted strings preserved"
fi

# Test 4: Non-empty quoted strings in command substitution should be preserved
QUOTED_STRING=$("$CJSH_PATH" -c 'echo "$(printf '"'"'test"X" value'"'"')"' 2>&1)
if [ "$QUOTED_STRING" != "test\"X\" value" ]; then
  echo "FAIL: quoted strings were not preserved (got: $QUOTED_STRING)"
  exit 1
else
  echo "PASS: quoted strings preserved"
fi

# Test 5: Case statement patterns should work in command substitution
CASE_TEST=$("$CJSH_PATH" -c 'sh -c "$(printf '"'"'opt=y\ncase $opt in\n  [Yy]*) echo matched;;\nesac'"'"')"' 2>&1)
if [ "$CASE_TEST" != "matched" ]; then
  echo "FAIL: case statement in command substitution failed (got: $CASE_TEST)"
  exit 1
else
  echo "PASS: case statement with patterns works"
fi

# Test 6: Case statement with empty string pattern (oh-my-zsh style)
CASE_EMPTY=$("$CJSH_PATH" -c 'sh -c "$(printf '"'"'opt=y\ncase $opt in\n  [Yy]*|"") echo matched;;\n  *) echo no match;;\nesac'"'"')"' 2>&1)
if [ "$CASE_EMPTY" != "matched" ]; then
  echo "FAIL: case statement with empty pattern failed (got: $CASE_EMPTY)"
  exit 1
else
  echo "PASS: case statement with empty string pattern works"
fi

# Test 7: Multi-line scripts via command substitution
MULTILINE=$("$CJSH_PATH" -c 'sh -c "$(printf '"'"'#!/bin/sh\necho hello\necho world'"'"')"' 2>&1)
EXPECTED_MULTILINE="hello
world"
if [ "$MULTILINE" != "$EXPECTED_MULTILINE" ]; then
  echo "FAIL: multi-line script via command substitution failed (got: $MULTILINE)"
  exit 1
else
  echo "PASS: multi-line scripts work"
fi

# Test 8: Backslashes in command substitution output should be preserved
BACKSLASH=$("$CJSH_PATH" -c 'echo "$(printf '"'"'test\\nwith\\\\backslash'"'"')"' 2>&1)
# printf interprets \n as literal backslash-n and \\\\ as two backslashes
EXPECTED_BACKSLASH="test\\nwith\\\\backslash"
if [ "$BACKSLASH" != "$EXPECTED_BACKSLASH" ]; then
  echo "FAIL: backslashes not preserved (got: $BACKSLASH, expected: $EXPECTED_BACKSLASH)"
  exit 1
else
  echo "PASS: backslashes preserved"
fi

# Test 9: Dollar signs followed by various characters should be preserved
DOLLAR_TEST=$("$CJSH_PATH" -c 'echo "$(printf '"'"'$var $() $$ $@ $*'"'"')"' 2>&1)
if [ "$DOLLAR_TEST" != "\$var \$() \$\$ \$@ \$*" ]; then
  echo "FAIL: dollar signs not preserved (got: $DOLLAR_TEST)"
  exit 1
else
  echo "PASS: dollar signs with various characters preserved"
fi

# Test 10: Backticks in command substitution output should be preserved
BACKTICK=$("$CJSH_PATH" -c 'echo "$(printf '"'"'test `command` here'"'"')"' 2>&1)
if [ "$BACKTICK" != "test \`command\` here" ]; then
  echo "FAIL: backticks not preserved (got: $BACKTICK)"
  exit 1
else
  echo "PASS: backticks preserved"
fi

# Test 11: Complex shell script with read and case (oh-my-zsh install script scenario)
# Use printf to build the script to avoid heredoc syntax issues
COMPLEX_OUT=$("$CJSH_PATH" -c 'sh -c "$(printf '"'"'#!/bin/sh\nopt=y\ncase $opt in\n  [Yy]*|"") echo "yes or empty";;\n  [Nn]*) echo "no";;\n  *) echo "other";;\nesac'"'"')"' 2>&1)
if [ "$COMPLEX_OUT" != "yes or empty" ]; then
  echo "FAIL: complex script with case patterns failed (got: $COMPLEX_OUT)"
  exit 1
else
  echo "PASS: complex script with case patterns works"
fi

# Test 12: Nested quotes in command substitution
NESTED_QUOTES=$("$CJSH_PATH" -c 'echo "$(printf '"'"'x="test value"'"'"')"' 2>&1)
if [ "$NESTED_QUOTES" != "x=\"test value\"" ]; then
  echo "FAIL: nested quotes not preserved (got: $NESTED_QUOTES)"
  exit 1
else
  echo "PASS: nested quotes preserved"
fi

# Test 13: Single quotes in double-quoted command substitution
SINGLE_IN_DOUBLE=$("$CJSH_PATH" -c 'echo "$(printf "test '"'"'single'"'"' quotes")"' 2>&1)
if [ "$SINGLE_IN_DOUBLE" != "test 'single' quotes" ]; then
  echo "FAIL: single quotes in double-quoted context not preserved (got: $SINGLE_IN_DOUBLE)"
  exit 1
else
  echo "PASS: single quotes in double-quoted context preserved"
fi

# Test 14: Command substitution output should not undergo word splitting when quoted
WORD_SPLIT=$("$CJSH_PATH" -c 'x="$(printf '"'"'one   two   three'"'"')"; printf "%s" "$x"' 2>&1)
if [ "$WORD_SPLIT" != "one   two   three" ]; then
  echo "FAIL: word splitting occurred in quoted command substitution (got: $WORD_SPLIT)"
  exit 1
else
  echo "PASS: no word splitting in quoted command substitution"
fi

# Test 15: Special shell characters should be preserved
SPECIAL_CHARS=$("$CJSH_PATH" -c 'echo "$(printf '"'"'test & | ; < > ( ) { } [ ] ! * ?'"'"')"' 2>&1)
if [ "$SPECIAL_CHARS" != "test & | ; < > ( ) { } [ ] ! * ?" ]; then
  echo "FAIL: special characters not preserved (got: $SPECIAL_CHARS)"
  exit 1
else
  echo "PASS: special characters preserved"
fi

# Test 16: Hash/pound signs (comments) in output should be preserved
HASH_TEST=$("$CJSH_PATH" -c 'echo "$(printf '"'"'#!/bin/sh\n# This is a comment'"'"')"' 2>&1)
EXPECTED_HASH="#!/bin/sh
# This is a comment"
if [ "$HASH_TEST" != "$EXPECTED_HASH" ]; then
  echo "FAIL: hash signs not preserved (got: $HASH_TEST)"
  exit 1
else
  echo "PASS: hash signs preserved"
fi

# Test 17: Verify unquoted command substitution still allows word splitting
UNQUOTED_SPLIT=$("$CJSH_PATH" -c 'set -- $(printf "one two three"); echo $#' 2>&1)
if [ "$UNQUOTED_SPLIT" != "3" ]; then
  echo "FAIL: word splitting not working in unquoted command substitution (got: $UNQUOTED_SPLIT)"
  exit 1
else
  echo "PASS: word splitting works in unquoted command substitution"
fi

# Test 18: Variable expansion OUTSIDE command substitution should still work
OUTER_VAR=$("$CJSH_PATH" -c 'x=outer; echo "before $(printf inner) $x after"' 2>&1)
if [ "$OUTER_VAR" != "before inner outer after" ]; then
  echo "FAIL: variable expansion outside command substitution broken (got: $OUTER_VAR)"
  exit 1
else
  echo "PASS: variable expansion outside command substitution works"
fi

# Test 19: Backslash escaping in command substitution
# Test that backslashes are preserved correctly
BACKSLASH_TEST=$("$CJSH_PATH" -c 'printf "%s" "$(printf '"'"'a\\b\\\\c'"'"')"' 2>&1)
if [ "$BACKSLASH_TEST" != "a\\b\\\\c" ]; then
  echo "FAIL: backslash escaping not correct (got: $BACKSLASH_TEST)"
  exit 1
else
  echo "PASS: backslash escaping handled correctly"
fi

# Test 20: Real-world oh-my-zsh pattern test
OHMYZSH_PATTERN=$("$CJSH_PATH" -c 'sh -c "$(printf '"'"'opt=yes
case $opt in
  [Yy]es|[Yy]|"")
    echo "Installing"
    ;;
  [Nn]o|[Nn])
    echo "Skipping"
    ;;
  *)
    echo "Invalid"
    ;;
esac'"'"')"' 2>&1)
if [ "$OHMYZSH_PATTERN" != "Installing" ]; then
  echo "FAIL: oh-my-zsh style pattern matching failed (got: $OHMYZSH_PATTERN)"
  exit 1
else
  echo "PASS: oh-my-zsh style pattern matching works"
fi

exit 0
