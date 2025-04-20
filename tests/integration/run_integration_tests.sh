#!/bin/bash

# Main integration test runner script
set -e

# Create results directory
mkdir -p test-results

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/../../build"
TERMINAL_BIN="$BUILD_DIR/DevToolsTerminal"

# Check if DevToolsTerminal was built
if [ ! -f "$TERMINAL_BIN" ]; then
  echo "ERROR: DevToolsTerminal binary not found at $TERMINAL_BIN"
  exit 1
fi

# Run all test scripts
echo "Running basic command tests..."
"$SCRIPT_DIR/test_basic_commands.exp" "$TERMINAL_BIN" > test-results/basic_commands.log
if [ $? -eq 0 ]; then
  echo "✅ Basic command tests passed"
else
  echo "❌ Basic command tests failed"
  exit 1
fi

echo "Running AI command tests..."
"$SCRIPT_DIR/test_ai_commands.exp" "$TERMINAL_BIN" > test-results/ai_commands.log
if [ $? -eq 0 ]; then
  echo "✅ AI command tests passed"
else
  echo "❌ AI command tests failed"
  exit 1
fi

echo "Running user settings tests..."
"$SCRIPT_DIR/test_user_settings.exp" "$TERMINAL_BIN" > test-results/user_settings.log
if [ $? -eq 0 ]; then
  echo "✅ User settings tests passed"
else
  echo "❌ User settings tests failed"
  exit 1
fi

# All tests completed successfully
echo "All integration tests passed successfully!"
exit 0
