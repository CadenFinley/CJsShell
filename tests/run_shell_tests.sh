#!/usr/bin/env sh
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
export CJSH="$SCRIPT_DIR/../build/cjsh"
SHELL_TESTS_DIR="$SCRIPT_DIR/shell"
if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ] || [ -n "$CONTINUOUS_INTEGRATION" ]; then
    export CJSH_CI_MODE="true"
    echo "Running in CI mode - signal and TTY tests may be skipped"
fi
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'
TOTAL_FILES=0
FILES_PASS=0
FILES_FAIL=0
FILES_SKIP=0
WARNINGS=0
TOTAL_TESTS=0
TESTS_PASS=0
TESTS_FAIL=0
TESTS_SKIP=0
if [ ! -x "$CJSH" ]; then
    echo "${RED}Error: cjsh binary not found at $CJSH${NC}"
    echo "Please run './toolchain/build.sh' to build cjsh first."
    exit 1
fi
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
discover_test_files() {
    find "$SHELL_TESTS_DIR" -maxdepth 1 -name "test_*.sh" -type f | sort | while read -r test_file; do
        basename "$test_file" .sh
    done
}
run_test() {
    test_name=$1
    test_file="$SHELL_TESTS_DIR/${test_name}.sh"
    TOTAL_FILES=$((TOTAL_FILES+1))
    printf "  %-50s " "$test_name:"
    if [ -f "$test_file" ]; then
        output=$(sh "$test_file" "$CJSH" 2>&1)
        exit_code=$?
        clean_output=$(printf "%s\n" "$output" | awk '{gsub(/\033\[[0-9;]*[A-Za-z]/, ""); print}')
        subtests_passed=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])PASS([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_failed=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])FAIL([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_skipped=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])SKIP([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_total=$((subtests_passed + subtests_failed + subtests_skipped))
        TOTAL_TESTS=$((TOTAL_TESTS + subtests_total))
        TESTS_PASS=$((TESTS_PASS + subtests_passed))
        TESTS_FAIL=$((TESTS_FAIL + subtests_failed))
        TESTS_SKIP=$((TESTS_SKIP + subtests_skipped))
        if [ $exit_code -eq 0 ]; then
            if [ $subtests_skipped -gt 0 ]; then
                echo "${GREEN}PASS${NC} (${subtests_passed}/${subtests_total}, ${subtests_skipped} skipped)"
            else
                echo "${GREEN}PASS${NC} (${subtests_passed}/${subtests_total})"
            fi
            FILES_PASS=$((FILES_PASS+1))
        else
            if [ $subtests_skipped -gt 0 ]; then
                echo "${RED}FAIL${NC} (${subtests_passed}/${subtests_total}, ${subtests_failed} failed, ${subtests_skipped} skipped)"
            else
                echo "${RED}FAIL${NC} (${subtests_passed}/${subtests_total}, ${subtests_failed} failed)"
            fi
            fail_lines=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])FAIL([^A-Za-z0-9_]|$)/) {print}')
            if [ -n "$fail_lines" ]; then
                printf "%s\n" "$fail_lines" | while IFS= read -r line; do
                    echo "    ${RED}$line${NC}"
                done
            fi
            FILES_FAIL=$((FILES_FAIL+1))
        fi
        if [ $subtests_skipped -gt 0 ]; then
            skip_lines=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])SKIP([^A-Za-z0-9_]|$)/) {print}')
            if [ -n "$skip_lines" ]; then
                printf "%s\n" "$skip_lines" | while IFS= read -r line; do
                    echo "    ${YELLOW}$line${NC}"
                done
            fi
        fi
        if printf "%s\n" "$clean_output" | grep -q "WARNING"; then
            WARNINGS=$((WARNINGS+1))
            echo "    ${YELLOW}$(printf "%s\n" "$clean_output" | grep "WARNING")${NC}"
        fi
    else
        echo "${YELLOW}SKIP${NC} (file not found)"
        FILES_SKIP=$((FILES_SKIP+1))
    fi
}
echo "${YELLOW}=== Running All Shell Tests ===${NC}"
test_list=$(discover_test_files)
for test_name in $test_list; do
    run_test "$test_name"
done
echo ""
echo "${BLUE}Test Summary${NC}"
echo "${BLUE}============${NC}"
echo "Total individual tests: $TOTAL_TESTS"
echo "${GREEN}Passed: $TESTS_PASS${NC}"
if [ $TESTS_FAIL -gt 0 ]; then
    echo "${RED}Failed: $TESTS_FAIL${NC}"
fi
if [ $TESTS_SKIP -gt 0 ]; then
    echo "${YELLOW}Skipped: $TESTS_SKIP${NC}"
fi
echo ""
echo "Test files: $TOTAL_FILES"
echo "${GREEN}Files passed: $FILES_PASS${NC}"
echo "${RED}Files failed: $FILES_FAIL${NC}"
if [ $WARNINGS -gt 0 ]; then
    echo "${YELLOW}Warnings: $WARNINGS${NC}"
fi
if [ $TOTAL_TESTS -gt 0 ]; then
    TESTS_EXECUTED=$((TOTAL_TESTS - TESTS_SKIP))
    if [ $TESTS_EXECUTED -gt 0 ]; then
        PASS_PERCENTAGE=$(awk "BEGIN {printf \"%.2f\", ($TESTS_PASS / $TESTS_EXECUTED) * 100}")
        echo ""
        echo "Pass rate: ${PASS_PERCENTAGE}% ($TESTS_PASS/$TESTS_EXECUTED executed tests)"
    else
        PASS_PERCENTAGE="0"
    fi
else
    PASS_PERCENTAGE="0"
fi
if [ $FILES_FAIL -eq 0 ]; then
    echo ""
    echo "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo ""
    echo "${RED}Some tests failed. Please review the output above.${NC}"
    exit $FILES_FAIL
fi
