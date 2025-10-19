#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: background job execution..."
OUTPUT=$("$CJSH_PATH" -c "sleep 0.1 & echo done")
if echo "$OUTPUT" | grep -q "^done"; then
  echo "PASS"
  exit 0
else
  echo "FAIL: expected 'done', got '$OUTPUT'"
  exit 1
fi