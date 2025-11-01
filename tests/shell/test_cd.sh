#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
  CJSH_PATH="$CJSH"
else
  CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
echo "Test: builtin cd..."

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

OUTPUT=$("$CJSH_PATH" -c "cd /tmp; pwd")
if [ "$OUTPUT" = "/tmp" ] || [ "$OUTPUT" = "/private/tmp" ]; then
  pass_test "cd to /tmp"
else
  fail_test "cd to /tmp - expected '/tmp', got '$OUTPUT'"
  exit 1
fi

OUTPUT=$("$CJSH_PATH" -c 'my_cd(){ cd /tmp; }; my_cd; pwd')
if [ "$OUTPUT" = "/tmp" ] || [ "$OUTPUT" = "/private/tmp" ]; then
  pass_test "cd inside function updates PWD"
else
  fail_test "cd inside function - expected '/tmp', got '$OUTPUT'"
  exit 1
fi

AUTO_CD_TMP="$(mktemp -d 2>/dev/null)"
if [ -n "$AUTO_CD_TMP" ] && [ -d "$AUTO_CD_TMP" ]; then
  AUTO_CD_BIN="$AUTO_CD_TMP/bin"
  mkdir -p "$AUTO_CD_BIN" "$AUTO_CD_TMP/autocdtest"
  cat <<'EOF' >"$AUTO_CD_BIN/autocdtest"
#!/usr/bin/env sh
echo "ran command"
EOF
  chmod +x "$AUTO_CD_BIN/autocdtest"

  OUTPUT=$(cd "$AUTO_CD_TMP" && PATH="$AUTO_CD_BIN:$PATH" "$CJSH_PATH" -c 'orig="$(pwd)"; autocdtest; pwd')

  if printf "%s" "$OUTPUT" | grep -q "ran command" && [ "$(printf "%s" "$OUTPUT" | tail -n 1)" = "$AUTO_CD_TMP" ]; then
    pass_test "auto cd defers to executable when available"
  else
    fail_test "auto cd should not override existing command"
  fi

  rm -rf "$AUTO_CD_TMP"
else
  skip_test "auto cd precedence (mktemp unavailable)"
fi

echo ""
echo "CD Tests Summary:"
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