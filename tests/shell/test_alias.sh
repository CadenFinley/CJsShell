#!/usr/bin/env sh
# Test alias and unalias builtins
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: alias/unalias..."

OUT=$("$CJSH_PATH" -c "alias hi='echo hello'; hi")
if [ "$OUT" != "hello" ]; then
  echo "FAIL: alias expansion simple (got '$OUT')"
  exit 1
fi

OUT2=$("$CJSH_PATH" -c "alias say='echo'; say world")
if [ "$OUT2" != "world" ]; then
  echo "FAIL: alias with args (got '$OUT2')"
  exit 1
fi

OUT3=$("$CJSH_PATH" -c "alias hi='echo hello'; unalias hi; command -v hi >/dev/null 2>&1; echo $?" 2>/dev/null)
if [ "$OUT3" = "0" ]; then
  echo "FAIL: unalias did not remove alias"
  exit 1
fi

echo "PASS"
exit 0
