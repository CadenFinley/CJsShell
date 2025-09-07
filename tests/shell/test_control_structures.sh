#!/usr/bin/env sh
# Test control structures: if, for, while
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: control structures..."
IF_OUTPUT=$("$CJSH_PATH" -c "if [ 1 -eq 1 ]; then printf ok; else printf fail; fi")
if [ "$IF_OUTPUT" != "ok" ]; then
  echo "FAIL: if statement"
  exit 1
fi
FOR_OUTPUT=$("$CJSH_PATH" -c "count=0; for i in 1 2 3; do count=\$((count+i)); done; printf \$count")
if [ "$FOR_OUTPUT" != "6" ]; then
  echo "FAIL: for loop"
  exit 1
fi
WHILE_OUTPUT=$("$CJSH_PATH" -c "count=0; i=1; while [ \$i -le 3 ]; do count=\$((count+i)); i=\$((i+1)); done; printf \$count")
if [ "$WHILE_OUTPUT" != "6" ]; then
  echo "FAIL: while loop"
  exit 1
fi
echo "PASS"
exit 0