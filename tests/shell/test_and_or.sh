#!/usr/bin/env sh
# Test logical AND/OR operators
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: logical AND/OR..."

OUT1=$("$CJSH_PATH" -c "true && echo ok")
if [ "$OUT1" != "ok" ]; then
  echo "FAIL: true && echo ok -> '$OUT1'"
  exit 1
fi

OUT2=$("$CJSH_PATH" -c "false || echo ok")
if [ "$OUT2" != "ok" ]; then
  echo "FAIL: false || echo ok -> '$OUT2'"
  exit 1
fi

"$CJSH_PATH" -c "false && echo nope"
if [ $? -eq 0 ]; then
  echo "FAIL: false && echo nope should not succeed"
  exit 1
fi

echo "PASS"
exit 0
