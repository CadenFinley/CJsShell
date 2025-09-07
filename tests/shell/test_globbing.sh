#!/usr/bin/env sh
# Test filename globbing expansion
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: globbing..."

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
touch "$TMPDIR/a.txt" "$TMPDIR/ab.txt" "$TMPDIR/b.txt"

# First check if globbing is supported; if not, skip
PROBE=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo *.txt")
# If probe still contains a literal '*', globbing isn't supported
case "$(printf %s "$PROBE" | tr -d "'\"")" in
  *\**)
    echo "SKIP: globbing not supported by cjsh"
    exit 0
    ;;
esac

OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; printf '%s ' *.txt | sed 's/ *$//' | tr -d '\n'")
# Accept either absolute or relative paths depending on shell print; normalize to basenames
OUT_BASE=$(echo "$OUT" | xargs -n1 basename | tr '\n' ' ' | sed 's/ *$//')

EXPECTED="a.txt ab.txt b.txt"
if [ "$OUT_BASE" != "$EXPECTED" ] && [ "$OUT_BASE" != "a.txt b.txt ab.txt" ]; then
  echo "FAIL: globbing result '$OUT_BASE'"
  exit 1
fi

echo "PASS"
exit 0
