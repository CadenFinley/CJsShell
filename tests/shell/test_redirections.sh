#!/usr/bin/env sh
# Test IO redirections: >, >>, 2>, and 2>&1 into pipeline
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: redirections..."

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
FILE="$TMPDIR/out.txt"
ERRFILE="$TMPDIR/err.txt"

# Test > and >>; must be supported
"$CJSH_PATH" -c "echo one > '$FILE'; echo two >> '$FILE'" 2>/dev/null
if [ ! -s "$FILE" ]; then
  echo "FAIL: output file not created"
  exit 1
fi
OUT=$(tr -d '\r' < "$FILE")
OUT_STRIPPED=$(printf %s "$OUT" | sed -e 's/\n$//')
EXPECTED="one
two"
if [ "$OUT_STRIPPED" != "$EXPECTED" ]; then
  echo "FAIL: > and >> redirection (got: $(printf %s "$OUT" | tr '\n' '|'))"
  exit 1
fi

# Test stderr redirection
"$CJSH_PATH" -c "sh -c 'echo OOPS 1>&2' 2> '$ERRFILE'"
ERR=$(tr -d '\r\n' < "$ERRFILE")
if [ "$ERR" != "OOPS" ]; then
  echo "FAIL: 2> redirection (got '$ERR')"
  exit 1
fi

# Test 2>&1 merged into pipeline
MERGED=$("$CJSH_PATH" -c "sh -c 'echo OUT; echo ERR 1>&2' 2>&1 | sort" 2>/dev/null)
MERGED_TRIM=$(printf %s "$MERGED" | tr -d '\r')
EXPECTED_1="ERR
OUT"
EXPECTED_2="ERR
OUT
"
if [ "$MERGED_TRIM" != "$EXPECTED_1" ] && [ "$MERGED_TRIM" != "$EXPECTED_2" ]; then
  echo "FAIL: 2>&1 into pipeline (got: $(printf %s "$MERGED_TRIM" | tr '\n' '|'))"
  exit 1
fi

echo "PASS"
exit 0
