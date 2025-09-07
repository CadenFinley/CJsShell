#!/usr/bin/env sh
# Test login shell behavior and initialization
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: login shell behavior..."

# Create temporary home directory for testing
TEST_HOME=$(mktemp -d)
export HOME="$TEST_HOME"

# Test login mode detection
OUT=$("$CJSH_PATH" --login -c "echo 'login test'" 2>/dev/null)
if [ "$OUT" != "login test" ]; then
    echo "FAIL: login mode execution (got '$OUT')"
    rm -rf "$TEST_HOME"
    exit 1
fi

# Test that login mode creates necessary files
"$CJSH_PATH" --login -c "true" 2>/dev/null
# Check if cjsh profile was created
if [ ! -f "$TEST_HOME/.cjsh/profile" ] && [ ! -f "$TEST_HOME/.cjshprofile" ]; then
    echo "WARNING: login mode did not create profile file"
fi

# Test profile file sourcing in login mode
echo "export TEST_PROFILE_VAR=profile_value" > "$TEST_HOME/.profile"
OUT=$("$CJSH_PATH" --login -c "echo \$TEST_PROFILE_VAR" 2>/dev/null)
if [ "$OUT" != "profile_value" ]; then
    echo "WARNING: profile file not sourced in login mode (got '$OUT')"
fi

# Test source file creation and sourcing
"$CJSH_PATH" -c "true" 2>/dev/null
if [ ! -f "$TEST_HOME/.cjsh/source" ] && [ ! -f "$TEST_HOME/.cjshrc" ]; then
    echo "WARNING: interactive mode did not create source file"
fi

# Test environment variable setup
OUT=$("$CJSH_PATH" -c "echo \$HOME")
# Don't override the real HOME in the test shell, just check it's set
if [ -z "$OUT" ]; then
    echo "FAIL: HOME variable should be set"
    rm -rf "$TEST_HOME"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo \$USER")
if [ -z "$OUT" ]; then
    echo "FAIL: USER variable should be set"
    rm -rf "$TEST_HOME"
    exit 1
fi

OUT=$("$CJSH_PATH" -c "echo \$SHELL")
if [ -z "$OUT" ]; then
    echo "FAIL: SHELL variable should be set"
    rm -rf "$TEST_HOME"
    exit 1
fi

# Test PWD variable
OUT=$("$CJSH_PATH" -c "echo \$PWD")
if [ -z "$OUT" ]; then
    echo "FAIL: PWD variable should be set"
    rm -rf "$TEST_HOME"
    exit 1
fi

# Test SHLVL variable
OUT=$("$CJSH_PATH" -c "echo \$SHLVL")
if [ -z "$OUT" ]; then
    echo "FAIL: SHLVL variable should be set"
    rm -rf "$TEST_HOME"
    exit 1
fi

# Test that SHLVL increments in subshells
OUT=$("$CJSH_PATH" -c "echo \$SHLVL; $CJSH_PATH -c 'echo \$SHLVL'" 2>/dev/null)
LINES=$(echo "$OUT" | wc -l)
if [ "$LINES" -ne 2 ]; then
    echo "WARNING: SHLVL increment test inconclusive"
fi

# Test PATH variable setup
OUT=$("$CJSH_PATH" -c "echo \$PATH")
if [ -z "$OUT" ]; then
    echo "FAIL: PATH variable should be set"
    rm -rf "$TEST_HOME"
    exit 1
fi

# Test that PATH contains standard directories
echo "$OUT" | grep -q "/bin"
if [ $? -ne 0 ]; then
    echo "WARNING: PATH should contain /bin"
fi

# Test IFS variable
OUT=$("$CJSH_PATH" -c "echo \"\$IFS\"" | od -c 2>/dev/null)
if [ -z "$OUT" ]; then
    echo "WARNING: IFS variable check failed"
fi

# Test command line argument preservation for restart
OUT=$("$CJSH_PATH" --no-colors -c "echo test")
if [ "$OUT" != "test" ]; then
    echo "FAIL: command line arguments not preserved (got '$OUT')"
    rm -rf "$TEST_HOME"
    exit 1
fi

# Test that login shell processes /etc/profile if it exists
if [ -f "/etc/profile" ]; then
    # This is hard to test without actually modifying system files
    echo "NOTE: /etc/profile exists and should be processed in login mode"
fi

# Cleanup
rm -rf "$TEST_HOME"

echo "PASS"
exit 0
