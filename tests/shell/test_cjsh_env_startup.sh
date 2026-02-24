#!/usr/bin/env sh
if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: cjsh env startup files..."

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

TEST_HOME=$(mktemp -d)
export HOME="$TEST_HOME"
mkdir -p "$TEST_HOME/.config/cjsh"

cleanup() {
    rm -rf "$TEST_HOME"
}
trap cleanup EXIT INT TERM

unset CJSH_ENV

echo "export CJSH_ENV_MARK=env_home" > "$TEST_HOME/.cjshenv"
OUT=$("$CJSH_PATH" -c "echo \$CJSH_ENV_MARK" 2>/dev/null)
if [ "$OUT" = "env_home" ]; then
    pass_test ".cjshenv sourced from home"
else
    fail_test ".cjshenv sourced from home (got '$OUT')"
fi

rm -f "$TEST_HOME/.cjshenv"
echo "export CJSH_ENV_MARK=env_alt" > "$TEST_HOME/.config/cjsh/.cjshenv"
OUT=$("$CJSH_PATH" -c "echo \$CJSH_ENV_MARK" 2>/dev/null)
if [ "$OUT" = "env_alt" ]; then
    pass_test ".cjshenv sourced from config alt path"
else
    fail_test ".cjshenv sourced from config alt path (got '$OUT')"
fi

echo "export CJSH_ENV_MARK=env_home" > "$TEST_HOME/.cjshenv"
override_env="$TEST_HOME/override_env.cjsh"
echo "export CJSH_ENV_MARK=env_override" > "$override_env"
export CJSH_ENV="$override_env"
OUT=$("$CJSH_PATH" -c "echo \$CJSH_ENV_MARK" 2>/dev/null)
if [ "$OUT" = "env_override" ]; then
    pass_test "CJSH_ENV overrides default search paths"
else
    fail_test "CJSH_ENV overrides default search paths (got '$OUT')"
fi

export CJSH_ENV="$TEST_HOME/does_not_exist.cjsh"
unset CJSH_ENV_MARK
OUT=$("$CJSH_PATH" -c "echo \$CJSH_ENV_MARK" 2>/dev/null)
if [ -z "$OUT" ]; then
    pass_test "Invalid CJSH_ENV skips sourcing"
else
    fail_test "Invalid CJSH_ENV skips sourcing (got '$OUT')"
fi

unset CJSH_ENV
echo "export CJSH_ORDER=env" > "$TEST_HOME/.cjshenv"
echo "export CJSH_ORDER=\"\${CJSH_ORDER}-profile\"" > "$TEST_HOME/.cjprofile"
OUT=$("$CJSH_PATH" --login -c "echo \$CJSH_ORDER" 2>/dev/null)
if [ "$OUT" = "env-profile" ]; then
    pass_test "cjshenv sourced before cjprofile"
else
    fail_test "cjshenv sourced before cjprofile (got '$OUT')"
fi

echo ""
echo "=== Test Summary ==="
TOTAL_TESTS=$((TESTS_PASSED + TESTS_FAILED))

if [ $TESTS_FAILED -eq 0 ]; then
    echo "All tests passed! ($TESTS_PASSED/$TOTAL_TESTS)"
    exit 0
else
    echo "Some tests failed. ($TESTS_PASSED/$TOTAL_TESTS)"
    exit 1
fi
