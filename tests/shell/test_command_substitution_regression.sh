#!/usr/bin/env sh

# test_command_substitution_regression.sh
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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

# Test 21: Builtin stdout before $(...) should not contaminate substitution output
"$CJSH_PATH" -c 'cjshopt completion-case status; x=$(mktemp); [ -f "$x" ] && rm -f "$x"' >/dev/null 2>&1
if [ $? -eq 0 ]; then
  echo "PASS: builtin stdout does not leak into \$() substitution"
else
  echo "FAIL: builtin stdout leaked into \$() substitution"
  exit 1
fi

# Test 22: Builtin stdout before backticks should not contaminate substitution output
"$CJSH_PATH" -c 'cjshopt completion-case status; x=`mktemp`; [ -f "$x" ] && rm -f "$x"' >/dev/null 2>&1
if [ $? -eq 0 ]; then
  echo "PASS: builtin stdout does not leak into backtick substitution"
else
  echo "FAIL: builtin stdout leaked into backtick substitution"
  exit 1
fi

# Test 23: Polluted substitution must not break later redirections
"$CJSH_PATH" -c 'cjshopt completion-case status; target=$(mktemp); printf "ok" > "$target" && [ "$(cat "$target")" = "ok" ] && rm -f "$target"' >/dev/null 2>&1
if [ $? -eq 0 ]; then
  echo "PASS: command substitution output remains safe for redirections"
else
  echo "FAIL: command substitution contamination broke redirection filename handling"
  exit 1
fi

# Test 24: Command substitution result should stay single-line after prior builtin stdout
"$CJSH_PATH" -c 'cjshopt completion-case status; x=$(mktemp); lines=$(printf "%s\n" "$x" | wc -l | tr -d "[:space:]"); [ "$lines" = "1" ] && [ -f "$x" ] && rm -f "$x"' >/dev/null 2>&1
if [ $? -eq 0 ]; then
  echo "PASS: command substitution result remains a single filename"
else
  echo "FAIL: command substitution result became multi-line after builtin output"
  exit 1
fi

# Test 25: A quoted substitution can contain quoted pipeline syntax as literal data
QUOTED_PIPE=$(
  "$CJSH_PATH" -c 'printf "%s" "$(printf '\''grep -E "left|right"'\'')"' 2>&1
)
if [ "$QUOTED_PIPE" = 'grep -E "left|right"' ]; then
  echo "PASS: quoted pipe from command substitution remains literal"
else
  echo "FAIL: quoted pipe from command substitution was reparsed (got: $QUOTED_PIPE)"
  exit 1
fi

# Test 26: Substitution output must not undergo another expansion pass
LITERAL_EXPANSIONS=$(
  "$CJSH_PATH" -c 'printf "%s" "$(printf '\''${HOME} $((1 + 2))'\'')"' 2>&1
)
if [ "$LITERAL_EXPANSIONS" = '${HOME} $((1 + 2))' ]; then
  echo "PASS: parameter and arithmetic syntax in substitution output remains literal"
else
  echo "FAIL: substitution output was expanded again (got: $LITERAL_EXPANSIONS)"
  exit 1
fi

# Test 27: Escaped quotes must keep a pipe inside the surrounding quoted argument
ESCAPED_QUOTED_PIPE=$(
  "$CJSH_PATH" -c 'printf "%s" "left\"|\"right"' 2>&1
)
if [ "$ESCAPED_QUOTED_PIPE" = 'left"|"right' ]; then
  echo "PASS: escaped quotes protect a literal pipe from pipeline parsing"
else
  echo "FAIL: pipe between escaped quotes was parsed (got: $ESCAPED_QUOTED_PIPE)"
  exit 1
fi

# Test 28: The same escaped-quote handling must apply to semicolon parsing
ESCAPED_QUOTED_SEMICOLON=$(
  "$CJSH_PATH" -c 'printf "%s" "left\";\"right"' 2>&1
)
if [ "$ESCAPED_QUOTED_SEMICOLON" = 'left";"right' ]; then
  echo "PASS: escaped quotes protect a literal semicolon"
else
  echo "FAIL: semicolon between escaped quotes was parsed (got: $ESCAPED_QUOTED_SEMICOLON)"
  exit 1
fi

# Test 29: Logical operators between escaped quotes must remain literal data
ESCAPED_QUOTED_LOGICAL=$(
  "$CJSH_PATH" -c 'printf "%s" "left\"&&\"middle\"||\"right"' 2>&1
)
if [ "$ESCAPED_QUOTED_LOGICAL" = 'left"&&"middle"||"right' ]; then
  echo "PASS: escaped quotes protect literal logical operators"
else
  echo "FAIL: logical operator between escaped quotes was parsed (got: $ESCAPED_QUOTED_LOGICAL)"
  exit 1
fi

# Test 30: Escaped operators outside quotes must also bypass command splitting
ESCAPED_OPERATORS=$(
  "$CJSH_PATH" -c 'printf "%s" left\|middle\;right' 2>&1
)
if [ "$ESCAPED_OPERATORS" = 'left|middle;right' ]; then
  echo "PASS: backslash-escaped operators remain in one argument"
else
  echo "FAIL: backslash-escaped operator was parsed (got: $ESCAPED_OPERATORS)"
  exit 1
fi

# Test 31: Protection must resume normal expansion after multiple substitution regions
MULTIPLE_PROTECTED=$(
  "$CJSH_PATH" -c 'name=outer; printf "%s" "$(printf '\''${name}'\'')|$(printf '\''$((2+3))'\'')|${name}"' 2>&1
)
if [ "$MULTIPLE_PROTECTED" = '${name}|$((2+3))|outer' ]; then
  echo "PASS: adjacent protected regions do not block surrounding expansion"
else
  echo "FAIL: protected-region boundary handling was incorrect (got: $MULTIPLE_PROTECTED)"
  exit 1
fi

# Test 32: Nested quoted substitutions must preserve syntax emitted by the innermost command
NESTED_PROTECTED=$(
  "$CJSH_PATH" -c 'name=outer; printf "%s" "$(printf "%s" "$(printf '\''${name} "a|b"'\'')")"' 2>&1
)
if [ "$NESTED_PROTECTED" = '${name} "a|b"' ]; then
  echo "PASS: nested substitution output remains opaque"
else
  echo "FAIL: nested substitution output was reparsed (got: $NESTED_PROTECTED)"
  exit 1
fi

# Test 33: Backtick substitutions use the same protected-output behavior
BACKTICK_PROTECTED=$(
  "$CJSH_PATH" -c 'printf "%s" "`printf '\''${HOME} $((1+2)) "left|right"'\''`"' 2>&1
)
if [ "$BACKTICK_PROTECTED" = '${HOME} $((1+2)) "left|right"' ]; then
  echo "PASS: backtick substitution output remains opaque"
else
  echo "FAIL: backtick substitution output was reparsed (got: $BACKTICK_PROTECTED)"
  exit 1
fi

# Test 34: Unquoted output is split into fields without being expanded a second time
UNQUOTED_PROTECTED=$(
  "$CJSH_PATH" -c 'name=outer; set -- $(printf '\''${name}:$((1+2)) two'\''); printf "%s|%s|%s" "$#" "$1" "$2"' 2>&1
)
if [ "$UNQUOTED_PROTECTED" = '2|${name}:$((1+2))|two' ]; then
  echo "PASS: unquoted substitution splits fields without re-expansion"
else
  echo "FAIL: unquoted substitution output was re-expanded (got: $UNQUOTED_PROTECTED)"
  exit 1
fi

# Test 35: A multiline Bash script must reach bash intact and expand in the child shell
BASH_SCRIPT_OUTPUT=$(
  "$CJSH_PATH" -c 'payload=outer; /bin/bash -c "$(printf '\''payload=inner\nif echo "$payload" | grep -Eq "inner|other"; then\n  echo "${payload}|$((1 + 2))"\nfi'\'')"' 2>&1
)
if [ "$BASH_SCRIPT_OUTPUT" = 'inner|3' ]; then
  echo "PASS: multiline bash -c payload is parsed and expanded by bash"
else
  echo "FAIL: multiline bash -c payload was modified by cjsh (got: $BASH_SCRIPT_OUTPUT)"
  exit 1
fi

# Test 36: Installer-style content must survive command substitution byte-for-byte
INSTALLER_FRAGMENT=$(
  "$CJSH_PATH" -c 'printf "%s" "$(printf '\''#!/bin/bash
# Allow `[[ -n "$(command)" ]]`, `func "$(command)"`, pipes, etc.
[[ -f /proc/1/cgroup ]] && grep -E "alpha|beta|gamma" -q /proc/1/cgroup && return
value="${HOME:-"/tmp"}"
count=$((1 + 2))'\'')"' 2>&1
)
EXPECTED_INSTALLER_FRAGMENT='#!/bin/bash
# Allow `[[ -n "$(command)" ]]`, `func "$(command)"`, pipes, etc.
[[ -f /proc/1/cgroup ]] && grep -E "alpha|beta|gamma" -q /proc/1/cgroup && return
value="${HOME:-"/tmp"}"
count=$((1 + 2))'
if [ "$INSTALLER_FRAGMENT" = "$EXPECTED_INSTALLER_FRAGMENT" ]; then
  echo "PASS: installer-style substitution output is byte-exact"
else
  echo "FAIL: installer-style substitution output changed"
  printf '      expected: %s\n' "$EXPECTED_INSTALLER_FRAGMENT"
  printf '      got:      %s\n' "$INSTALLER_FRAGMENT"
  exit 1
fi

# Test 37: Brace syntax emitted by an unquoted substitution is not brace-expanded
UNQUOTED_BRACES=$(
  "$CJSH_PATH" -c 'set -- $(printf '\''{left,right}'\''); printf "%s|%s" "$#" "$1"' 2>&1
)
if [ "$UNQUOTED_BRACES" = '1|{left,right}' ]; then
  echo "PASS: unquoted substitution output bypasses brace expansion"
else
  echo "FAIL: braces from substitution output were expanded (got: $UNQUOTED_BRACES)"
  exit 1
fi

# Test 38: Even backslashes before a closing quote must not hide the closing delimiter
TRAILING_BACKSLASH=$(
  "$CJSH_PATH" -c 'printf "%s" "prefix$(printf '\''tail\\'\'')suffix"' 2>&1
)
if [ "$TRAILING_BACKSLASH" = 'prefixtail\suffix' ]; then
  echo "PASS: trailing backslash from command substitution is preserved"
else
  echo "FAIL: trailing backslash confused substitution parsing (got: $TRAILING_BACKSLASH)"
  exit 1
fi

# Test 39: Protected unquoted output must not trigger nounset errors
NOUNSET_PROTECTED=$(
  "$CJSH_PATH" -c 'set -u; set -- $(printf '\''${CJSH_REGRESSION_UNSET}'\''); printf "%s|%s" "$#" "$1"' 2>&1
)
if [ "$NOUNSET_PROTECTED" = '1|${CJSH_REGRESSION_UNSET}' ]; then
  echo "PASS: unquoted protected output is opaque under nounset"
else
  echo "FAIL: nounset evaluated protected substitution output (got: $NOUNSET_PROTECTED)"
  exit 1
fi

# Test 40: Pipeline execution must retain protection and unquoted field splitting
PIPELINE_UNQUOTED=$(
  "$CJSH_PATH" -c 'name=outer; printf "<%s>\n" $(printf '\''${name} two'\'') | cat' 2>&1
)
EXPECTED_PIPELINE_UNQUOTED='<${name}>
<two>'
if [ "$PIPELINE_UNQUOTED" = "$EXPECTED_PIPELINE_UNQUOTED" ]; then
  echo "PASS: pipeline arguments preserve unquoted substitution semantics"
else
  echo "FAIL: pipeline reparsed unquoted substitution output (got: $PIPELINE_UNQUOTED)"
  exit 1
fi

exit 0
