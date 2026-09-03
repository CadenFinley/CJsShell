#!/usr/bin/env sh

# test_firstboot.sh
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

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: firstboot builtin..."

TEST_HOME=$(mktemp -d 2>/dev/null)
if [ -z "$TEST_HOME" ] || [ ! -d "$TEST_HOME" ]; then
    echo "FAIL: unable to create temporary home directory"
    exit 1
fi

MARKER_PATH="$TEST_HOME/.cache/cjsh/.first_boot"
FIRST_OUTPUT=$(HOME="$TEST_HOME" "$CJSH_PATH" -c "firstboot" 2>&1)
FIRST_STATUS=$?

if [ "$FIRST_STATUS" -eq 0 ] && [ -f "$MARKER_PATH" ] && [ ! -s "$MARKER_PATH" ]; then
    echo "PASS: firstboot creates the marker"
else
    echo "FAIL: firstboot did not create the marker (status=$FIRST_STATUS, output='$FIRST_OUTPUT')"
    rm -rf "$TEST_HOME"
    exit 1
fi

SECOND_OUTPUT=$(HOME="$TEST_HOME" "$CJSH_PATH" -c "firstboot" 2>&1)
SECOND_STATUS=$?

if [ "$SECOND_STATUS" -ne 0 ] && printf '%s' "$SECOND_OUTPUT" | grep -q "command not found"; then
    echo "PASS: firstboot is unavailable after creating the marker"
else
    echo "FAIL: firstboot remained available (status=$SECOND_STATUS, output='$SECOND_OUTPUT')"
    rm -rf "$TEST_HOME"
    exit 1
fi

BUILTIN_OUTPUT=$(HOME="$TEST_HOME" "$CJSH_PATH" -c "builtin firstboot" 2>&1)
BUILTIN_STATUS=$?
rm -rf "$TEST_HOME"

if [ "$BUILTIN_STATUS" -ne 0 ] && printf '%s' "$BUILTIN_OUTPUT" | grep -q "not a builtin"; then
    echo "PASS: builtin dispatch cannot bypass firstboot removal"
    echo "PASS"
    exit 0
fi

echo "FAIL: builtin dispatch could still access firstboot (status=$BUILTIN_STATUS, output='$BUILTIN_OUTPUT')"
exit 1
