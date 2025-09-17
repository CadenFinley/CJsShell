#!/usr/bin/env sh

# Generate test badge data from test results
# This script runs the test suite and extracts pass/fail counts for badge generation

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT="$SCRIPT_DIR/.."
TEST_SCRIPT="$PROJECT_ROOT/tests/run_shell_tests.sh"

# Colors for badge (URL encoded)
GREEN="%23success"
RED="%23critical" 
YELLOW="%23important"

# Run tests and capture output
echo "Running test suite to generate badge data..."
TEST_OUTPUT=$(cd "$PROJECT_ROOT" && "$TEST_SCRIPT" 2>&1)
EXIT_CODE=$?

# Extract test counts from the output (strip ANSI color codes first)
CLEAN_OUTPUT=$(echo "$TEST_OUTPUT" | sed 's/\x1b\[[0-9;]*m//g')

TOTAL_TESTS=$(echo "$CLEAN_OUTPUT" | grep "Total individual tests:" | sed 's/Total individual tests: //')
TESTS_PASSED=$(echo "$CLEAN_OUTPUT" | grep "^Passed:" | sed 's/Passed: //')
TESTS_FAILED=$(echo "$CLEAN_OUTPUT" | grep "^Failed:" | sed 's/Failed: //')
TESTS_SKIPPED=$(echo "$CLEAN_OUTPUT" | grep "^Skipped:" | sed 's/Skipped: //')

# Handle empty values (set to 0 if not found)
TOTAL_TESTS=${TOTAL_TESTS:-0}
TESTS_PASSED=${TESTS_PASSED:-0}
TESTS_FAILED=${TESTS_FAILED:-0}
TESTS_SKIPPED=${TESTS_SKIPPED:-0}

echo "Test Results:"
echo "  Total: $TOTAL_TESTS"
echo "  Passed: $TESTS_PASSED"
echo "  Failed: $TESTS_FAILED"
echo "  Skipped: $TESTS_SKIPPED"

# Determine badge color based on results
if [ "$TESTS_FAILED" -gt 0 ]; then
    BADGE_COLOR=$RED
elif [ "$TESTS_SKIPPED" -gt 0 ]; then
    BADGE_COLOR=$YELLOW
else
    BADGE_COLOR=$GREEN
fi

# Create badge label and message
BADGE_LABEL="tests"
BADGE_MESSAGE="${TESTS_PASSED}%2F${TOTAL_TESTS}%20passed"

# Generate shields.io URL
BADGE_URL="https://img.shields.io/badge/${BADGE_LABEL}-${BADGE_MESSAGE}-${BADGE_COLOR}"

echo ""
echo "Generated badge URL:"
echo "$BADGE_URL"

# Create badge markdown
BADGE_MARKDOWN="[![Tests](${BADGE_URL})](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)"

echo ""
echo "Badge Markdown:"
echo "$BADGE_MARKDOWN"

# Save results to files for use in CI
mkdir -p "$PROJECT_ROOT/build"
echo "$BADGE_URL" > "$PROJECT_ROOT/build/badge_url.txt"
echo "$BADGE_MARKDOWN" > "$PROJECT_ROOT/build/badge_markdown.txt"

# Create JSON for more advanced badge services if needed
cat > "$PROJECT_ROOT/build/test_results.json" << EOF
{
  "schemaVersion": 1,
  "label": "tests",
  "message": "${TESTS_PASSED}/${TOTAL_TESTS} passed",
  "color": "$([ "$TESTS_FAILED" -gt 0 ] && echo "red" || ([ "$TESTS_SKIPPED" -gt 0 ] && echo "yellow" || echo "green"))"
}
EOF

echo ""
echo "Badge data saved to:"
echo "  - build/badge_url.txt"
echo "  - build/badge_markdown.txt" 
echo "  - build/test_results.json"

# In CI environment, always exit successfully after generating badge data
# The badge itself shows the test status, but we don't want failed tests to fail the build
if [ "$CI" = "true" ] || [ "$APPVEYOR" = "true" ]; then
    echo "Running in CI environment - exiting successfully after generating badge data"
    exit 0
else
    # In local development, exit with the test result code
    exit $EXIT_CODE
fi
