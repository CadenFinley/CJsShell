#!/usr/bin/env sh
# Test quoting and expansions
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: quoting and expansions..."
VAR_OUTPUT=$("$CJSH_PATH" -c "var=world; printf \"hello \$var\"")
if [ "$VAR_OUTPUT" != "hello world" ]; then
  echo "FAIL: variable expansion"
  exit 1
fi
SINGLE_OUTPUT=$("$CJSH_PATH" -c "var=world; printf 'hello \$var'")
if [ "$SINGLE_OUTPUT" != "hello \$var" ]; then
  echo "FAIL: single quotes"
  exit 1
fi
CMD_OUTPUT=$("$CJSH_PATH" -c "printf \$(printf hello)")
if [ "$CMD_OUTPUT" != "hello" ]; then
  echo "FAIL: command substitution"
  exit 1
fi
ARITH_OUTPUT=$("$CJSH_PATH" -c "printf \$((1+2))")
if [ "$ARITH_OUTPUT" != "3" ]; then
  echo "FAIL: arithmetic expansion"
  exit 1
fi
echo "PASS"
exit 0