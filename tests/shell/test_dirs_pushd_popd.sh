#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
  CJSH_PATH="$CJSH"
else
  CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi
CJSH_CMD="$CJSH_PATH --no-smart-cd"

echo "Test: dirs/pushd/popd..."

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

TMPROOT="$(mktemp -d 2>/dev/null)"
if [ -z "$TMPROOT" ] || [ ! -d "$TMPROOT" ]; then
  TMPROOT="$(mktemp -d -t cjsh_dirs 2>/dev/null)"
fi

if [ -z "$TMPROOT" ] || [ ! -d "$TMPROOT" ]; then
  fail_test "mktemp for directory stack tests"
  exit 1
fi

cleanup() {
  rm -rf "$TMPROOT"
}
trap cleanup EXIT

mkdir -p "$TMPROOT/one" "$TMPROOT/two"

ROOT_DIR="$(cd "$TMPROOT" && pwd -P)"
DIR_ONE="$(cd "$TMPROOT/one" && pwd -P)"
DIR_TWO="$(cd "$TMPROOT/two" && pwd -P)"

OUT=$($CJSH_CMD -c "cd \"$ROOT_DIR\"; dirs")
if [ "$OUT" = "$ROOT_DIR" ]; then
  pass_test "dirs shows current directory"
else
  fail_test "dirs shows current directory (got '$OUT')"
fi

OUT=$($CJSH_CMD -c "cd \"$ROOT_DIR\"; pushd \"$DIR_ONE\" >/dev/null; dirs")
if [ "$OUT" = "$DIR_ONE $ROOT_DIR" ]; then
  pass_test "pushd adds current directory to stack"
else
  fail_test "pushd adds current directory to stack (got '$OUT')"
fi

OUT=$($CJSH_CMD -c "cd \"$ROOT_DIR\"; pushd \"$DIR_ONE\" >/dev/null; pushd \"$DIR_TWO\" >/dev/null; dirs")
if [ "$OUT" = "$DIR_TWO $DIR_ONE $ROOT_DIR" ]; then
  pass_test "pushd twice orders stack correctly"
else
  fail_test "pushd twice orders stack correctly (got '$OUT')"
fi

OUT=$($CJSH_CMD -c "cd \"$ROOT_DIR\"; pushd \"$DIR_ONE\" >/dev/null; pushd >/dev/null; dirs")
if [ "$OUT" = "$ROOT_DIR $DIR_ONE" ]; then
  pass_test "pushd with no args swaps current and stack top"
else
  fail_test "pushd with no args swaps current and stack top (got '$OUT')"
fi

OUT=$($CJSH_CMD -c "cd \"$ROOT_DIR\"; pushd \"$DIR_ONE\" >/dev/null; pushd \"$DIR_TWO\" >/dev/null; popd >/dev/null; dirs")
if [ "$OUT" = "$DIR_ONE $ROOT_DIR" ]; then
  pass_test "popd removes top of stack"
else
  fail_test "popd removes top of stack (got '$OUT')"
fi

$CJSH_CMD -c "cd \"$ROOT_DIR\"; popd" >/dev/null 2>&1
if [ $? -eq 1 ]; then
  pass_test "popd on empty stack returns error"
else
  fail_test "popd on empty stack should return error"
fi

$CJSH_CMD -c "cd \"$ROOT_DIR\"; pushd" >/dev/null 2>&1
if [ $? -eq 1 ]; then
  pass_test "pushd with empty stack returns error"
else
  fail_test "pushd with empty stack should return error"
fi

$CJSH_CMD -c "dirs extra" >/dev/null 2>&1
if [ $? -eq 2 ]; then
  pass_test "dirs with too many args returns error"
else
  fail_test "dirs with too many args should return error"
fi

OUT=$($CJSH_CMD -c "cd \"$ROOT_DIR\"; pushd \"$DIR_ONE\" >/dev/null; pushd \"$ROOT_DIR/nope\" >/dev/null 2>&1; dirs")
if [ "$OUT" = "$DIR_ONE $ROOT_DIR" ]; then
  pass_test "failed pushd does not alter stack"
else
  fail_test "failed pushd should not alter stack (got '$OUT')"
fi

echo ""
echo "Directory Stack Tests Summary:"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
  echo "PASS"
  exit 0
else
  echo "FAIL"
  exit 1
fi
