#!/usr/bin/env sh
# Run all shell tests for cjsh
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# Use the built cjsh binary for testing
export CJSH="$SCRIPT_DIR/../build/cjsh"
SHELL_TESTS_DIR="$SCRIPT_DIR/shell"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

TOTAL=0
PASS=0
FAIL=0
SKIP=0
WARNINGS=0

# Check if cjsh binary exists
if [ ! -x "$CJSH" ]; then
    echo "${RED}Error: cjsh binary not found at $CJSH${NC}"
    echo "Please run 'make' or 'cmake --build build' to build cjsh first."
    exit 1
fi

# Check for existing cjsh processes
echo "Checking for existing cjsh processes..."
EXISTING_CJSH=$(pgrep -f "cjsh" 2>/dev/null || true)
if [ -n "$EXISTING_CJSH" ]; then
    echo "${RED}Error: Found running cjsh process(es): $EXISTING_CJSH${NC}"
    echo "Please terminate any running cjsh instances before running tests."
    echo "You can use: pkill -f cjsh"
    exit 1
else
    echo "No existing cjsh processes found."
fi

echo "${BLUE}Running CJ's Shell Test Suite${NC}"
echo "${BLUE}=============================${NC}"
echo "Testing binary: $CJSH"
echo "Includes comprehensive POSIX compliance tests"
echo "This can take a second to complete. Hold tight."
echo ""

# Automatically discover all test files that start with "test_"
discover_test_files() {
    find "$SHELL_TESTS_DIR" -maxdepth 1 -name "test_*.sh" -type f | sort | while read -r test_file; do
        basename "$test_file" .sh
    done
}

run_test() {
    test_name=$1
    test_file="$SHELL_TESTS_DIR/${test_name}.sh"
    TOTAL=$((TOTAL+1))
    printf "  %-50s " "$test_name:"
    
    if [ -f "$test_file" ]; then
        # Capture both output and exit code
        output=$(sh "$test_file" "$CJSH" 2>&1)
        exit_code=$?
        
        # Count subtests within the test file
        subtests_passed=$(echo "$output" | grep -c "PASS")
        subtests_failed=$(echo "$output" | grep -c "FAIL")
        subtests_skipped=$(echo "$output" | grep -c "SKIP")
        subtests_total=$((subtests_passed + subtests_failed + subtests_skipped))
        
        if [ $exit_code -eq 0 ]; then
            if [ $subtests_skipped -gt 0 ]; then
                echo "${GREEN}PASS${NC} (${subtests_passed}/${subtests_total}, ${subtests_skipped} skipped)"
            else
                echo "${GREEN}PASS${NC} (${subtests_passed}/${subtests_total})"
            fi
            PASS=$((PASS+1))
        else
            echo "${RED}FAIL${NC} (${subtests_passed}/${subtests_total}, ${subtests_failed} failed)"
            echo "    Output: $output"
            FAIL=$((FAIL+1))
        fi
        
        # Show skipped tests details
        if [ $subtests_skipped -gt 0 ]; then
            skip_lines=$(echo "$output" | grep "SKIP")
            if [ -n "$skip_lines" ]; then
                echo "$skip_lines" | while IFS= read -r line; do
                    echo "    ${YELLOW}$line${NC}"
                done
            fi
        fi
        
        # Check for warnings in output
        if echo "$output" | grep -q "WARNING"; then
            WARNINGS=$((WARNINGS+1))
            echo "    ${YELLOW}$(echo "$output" | grep "WARNING")${NC}"
        fi
    else
        echo "${YELLOW}SKIP${NC} (file not found)"
        SKIP=$((SKIP+1))
    fi
}

# Run all discovered tests
echo "${YELLOW}=== Running All Shell Tests ===${NC}"
test_list=$(discover_test_files)
for test_name in $test_list; do
    run_test "$test_name"
done

echo ""
echo "${BLUE}Test Summary${NC}"
echo "${BLUE}============${NC}"
echo "Total tests: $TOTAL"
echo "${GREEN}Passed: $PASS${NC}"
echo "${RED}Failed: $FAIL${NC}"
if [ $WARNINGS -gt 0 ]; then
    echo "${YELLOW}Warnings: $WARNINGS${NC}"
fi

if [ $FAIL -eq 0 ]; then
    echo ""
    echo "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo ""
    echo "${RED}Some tests failed. Please review the output above.${NC}"
    exit $FAIL
fi