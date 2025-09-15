#!/usr/bin/env sh
# Test misc edges: PATH resolution, comments/whitespace, pipeline status
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: misc edges..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

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

# Comments and extra whitespace should be ignored
OUT=$("$CJSH_PATH" -c "   echo   hi   # this is a comment")
if [ "$OUT" != "hi" ]; then
  fail_test "comment/whitespace parsing"
  exit 1
else
  pass_test "comment/whitespace parsing"
fi

# PATH resolution: ensure a temp script runs when on PATH
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT INT TERM
cat > "$TMPDIR/hello.sh" <<'EOF'
#!/usr/bin/env sh
echo hi
EOF
chmod +x "$TMPDIR/hello.sh"
OUT2=$(PATH="$TMPDIR:$PATH" "$CJSH_PATH" -c "hello.sh")
if [ "$OUT2" != "hi" ]; then
  fail_test "PATH resolution (got '$OUT2')"
  exit 1
else
  pass_test "PATH resolution"
fi

# Pipeline exit status: probe behavior; fail if pipeline exit codes not supported
"$CJSH_PATH" -c "false | true"
EC1=$?
"$CJSH_PATH" -c "true | false"
EC2=$?
if [ $EC1 -eq 0 ] && [ $EC2 -ne 0 ]; then
  pass_test "pipeline exit status semantics"
else
  fail_test "pipeline exit status semantics differ or unsupported"
  exit 1
fi

echo ""
echo "Misc Edges Tests Summary:"
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
