#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
  CJSH_PATH="$CJSH"
else
  CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: builtin export..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

OUTPUT=$("$CJSH_PATH" -c "export FOO=bar; printf \"\$FOO\"")
if [ "$OUTPUT" = "bar" ]; then
  pass_test "export builtin"
else
  fail_test "export builtin - expected 'bar', got '$OUTPUT'"
  exit 1
fi

"$CJSH_PATH" -c "export \$0=blah && echo RUN" >/tmp/export_readonly_test.out 2>&1
EXPORT_STATUS=$?
if [ $EXPORT_STATUS -ne 0 ] && ! grep -q "RUN" /tmp/export_readonly_test.out && \
   grep -q "readonly variable" /tmp/export_readonly_test.out; then
  pass_test "export rejects special parameter assignment"
else
  fail_test "export rejects special parameter assignment"
fi

"$CJSH_PATH" -c "export \$0" >/tmp/export_readonly_noassign.out 2>&1
EXPORT_STATUS=$?
if [ $EXPORT_STATUS -ne 0 ] && grep -q "readonly variable" /tmp/export_readonly_noassign.out; then
  pass_test "export rejects special parameter name"
else
  fail_test "export rejects special parameter name"
fi

echo ""
echo "Export Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "PASS"
    exit 0
else
    echo "FAIL"
    exit 1
fi
