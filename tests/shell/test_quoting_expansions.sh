#!/usr/bin/env sh

# test_quoting_expansions.sh
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

if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: quoting and expansions..."
VAR_OUTPUT=$("$CJSH_PATH" -c "var=world; printf \"hello \$var\"")
if [ "$VAR_OUTPUT" != "hello world" ]; then
  echo "FAIL: variable expansion"
  exit 1
else
  echo "PASS: variable expansion"
fi
SINGLE_OUTPUT=$("$CJSH_PATH" -c "var=world; printf 'hello \$var'")
if [ "$SINGLE_OUTPUT" != "hello \$var" ]; then
  echo "FAIL: single quotes"
  exit 1
else
  echo "PASS: single quotes"
fi
CMD_OUTPUT=$("$CJSH_PATH" -c "printf \$(printf hello)")
if [ "$CMD_OUTPUT" != "hello" ]; then
  echo "FAIL: command substitution"
  exit 1
else
  echo "PASS: command substitution"
fi
ARITH_OUTPUT=$("$CJSH_PATH" -c "printf \$((1+2))")
if [ "$ARITH_OUTPUT" != "3" ]; then
  echo "FAIL: arithmetic expansion"
  exit 1
else
  echo "PASS: arithmetic expansion"
fi
echo "PASS"
exit 0