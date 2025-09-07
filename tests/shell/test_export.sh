#!/usr/bin/env sh
# Test the 'export' builtin
if [ -n "$CJSH" ]; then
  CJSH_PATH="$CJSH"
else
  CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: builtin export..."
OUTPUT=$("$CJSH_PATH" -c "export FOO=bar; printf \"\$FOO\"")
if [ "$OUTPUT" = "bar" ]; then
  echo "PASS"
  exit 0
else
  echo "FAIL: expected 'bar', got '$OUTPUT'"
  exit 1
fi