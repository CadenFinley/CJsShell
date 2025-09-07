#!/usr/bin/env sh
# Test error and exit code handling
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: error and exit codes..."
"$CJSH_PATH" -c "exit 42"
if [ $? -ne 42 ]; then
  echo "FAIL: exit 42 returned $?"
  exit 1
fi
"$CJSH_PATH" -c "false"
if [ $? -ne 1 ]; then
  echo "FAIL: false returned $?"
  exit 1
fi
echo "PASS"
exit 0