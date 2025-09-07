#!/usr/bin/env sh
# Test pipelines and redirections
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: pipelines..."
OUTPUT=$("$CJSH_PATH" -c "printf 'hello' | sed s/hello/world/ | wc -c")
OUTPUT=$(echo "$OUTPUT" | tr -d '[:space:]')
if [ "$OUTPUT" = "5" ]; then
  echo "PASS"
  exit 0
else
  echo "FAIL: expected '5', got '$OUTPUT'"
  exit 1
fi