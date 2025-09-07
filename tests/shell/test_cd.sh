#!/usr/bin/env sh
# Test the 'cd' builtin
if [ -n "$CJSH" ]; then
  CJSH_PATH="$CJSH"
else
  CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: builtin cd..."
OUTPUT=$("$CJSH_PATH" -c "cd /tmp; pwd")
if [ "$OUTPUT" = "/tmp" ] || [ "$OUTPUT" = "/private/tmp" ]; then
  echo "PASS"
  exit 0
else
  echo "FAIL: expected '/tmp', got '$OUTPUT'"
  exit 1
fi