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

# Test cd to /tmp
OUTPUT=$("$CJSH_PATH" -c "cd /tmp; pwd")
if [ "$OUTPUT" = "/tmp" ] || [ "$OUTPUT" = "/private/tmp" ]; then
  pass_test "cd to /tmp"
else
  fail_test "cd to /tmp - expected '/tmp', got '$OUTPUT'"
  exit 1
fi

# Test cd inside a function preserves updated PWD
OUTPUT=$("$CJSH_PATH" -c 'my_cd(){ cd /tmp; }; my_cd; pwd')
if [ "$OUTPUT" = "/tmp" ] || [ "$OUTPUT" = "/private/tmp" ]; then
  pass_test "cd inside function updates PWD"
else
  fail_test "cd inside function - expected '/tmp', got '$OUTPUT'"
  exit 1
fi

# Test implicit cd by typing a directory path
TEMP_DIR=$(mktemp -d)
if [ ! -d "$TEMP_DIR" ]; then
  fail_test "mktemp failed to create temp directory"
  exit 1
fi

mkdir -p "$TEMP_DIR/auto-cd-target"
EXPECTED=$(python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$TEMP_DIR/auto-cd-target")
OUTPUT=$("$CJSH_PATH" -c "cd \"$TEMP_DIR\"; auto-cd-target; pwd")
if [ "$OUTPUT" = "$EXPECTED" ]; then
  pass_test "implicit cd when directory name is entered"
else
  fail_test "implicit cd - expected '$EXPECTED', got '$OUTPUT'"
  rm -rf "$TEMP_DIR"
  exit 1
fi

rm -rf "$TEMP_DIR"

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