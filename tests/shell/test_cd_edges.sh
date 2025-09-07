#!/usr/bin/env sh
# Test 'cd' edge cases: non-existent dir, HOME
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: cd edge cases..."

"$CJSH_PATH" -c "cd /definitely/not/a/real/path"
if [ $? -eq 0 ]; then
  echo "FAIL: cd to non-existent path should fail"
  exit 1
fi

HOME_OUT=$("$CJSH_PATH" -c "cd; pwd")
HOME_OS=${HOME#/private}
HOME_OUT_OS=${HOME_OUT#/private}
if [ "$HOME_OUT_OS" != "$HOME_OS" ]; then
  echo "FAIL: cd to HOME expected '$HOME', got '$HOME_OUT'"
  exit 1
fi

echo "PASS"
exit 0
