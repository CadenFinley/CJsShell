#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: globbing..."

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip_test() {
    echo "SKIP: $1"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
touch "$TMPDIR/a.txt" "$TMPDIR/ab.txt" "$TMPDIR/b.txt"

PROBE=$("$CJSH_PATH" -c "cd '$TMPDIR'; echo *.txt")
case "$(printf %s "$PROBE" | tr -d "'\"")" in
  *\**)
    fail_test "globbing not supported by cjsh"
    exit 1
    ;;
esac
pass_test "globbing support detected"

OUT=$("$CJSH_PATH" -c "cd '$TMPDIR'; printf '%s ' *.txt | sed 's/ *$//' | tr -d '\n'")
OUT_BASE=$(echo "$OUT" | xargs -n1 basename | tr '\n' ' ' | sed 's/ *$//')

EXPECTED="a.txt ab.txt b.txt"
if [ "$OUT_BASE" != "$EXPECTED" ] && [ "$OUT_BASE" != "a.txt b.txt ab.txt" ]; then
  fail_test "globbing result '$OUT_BASE'"
  exit 1
else
  pass_test "globbing expansion"
fi

echo ""
echo "Globbing Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
echo "Skipped: $TESTS_SKIPPED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
