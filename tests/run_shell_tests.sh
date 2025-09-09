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

echo "${BLUE}Running CJ's Shell Test Suite${NC}"
echo "${BLUE}=============================${NC}"
echo "Testing binary: $CJSH"
echo "Includes comprehensive POSIX compliance tests"
echo ""

# Define test categories and their tests (using simple variables instead of associative arrays)
CORE_TESTS="test_builtin_commands test_environment_vars test_command_line_options"
FILEIO_TESTS="test_file_operations test_redirections test_posix_io"
SCRIPTING_TESTS="test_scripting test_control_structures test_quoting_expansions test_posix_variables"
PROCESS_TESTS="test_process_management test_pipeline test_job_control test_and_or test_posix_signals test_zombie_processes test_zombie_edge_cases test_sigchld_handling"
SHELL_TESTS="test_alias test_cd test_cd_edges test_export"
FEATURES_TESTS="test_login_shell test_posix_login_env"
COMPLIANCE_TESTS="test_posix_compliance test_posix_advanced test_posix_builtins test_globbing"
EDGE_TESTS="test_error_handling test_error_edge_cases test_misc_edges"
PERFORMANCE_TESTS="test_performance"

run_test_category() {
    category=$1
    tests=$2
    echo "${YELLOW}=== $category Tests ===${NC}"
    
    for test_name in $tests; do
        test_file="$SHELL_TESTS_DIR/${test_name}.sh"
        TOTAL=$((TOTAL+1))
        printf "  %-30s " "$test_name:"
        
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
    done
    echo ""
}

# Run tests by category
run_test_category "Core" "$CORE_TESTS"
run_test_category "FileIO" "$FILEIO_TESTS"
run_test_category "Scripting" "$SCRIPTING_TESTS"
run_test_category "Process" "$PROCESS_TESTS"
run_test_category "Shell" "$SHELL_TESTS"
run_test_category "Features" "$FEATURES_TESTS"
run_test_category "Compliance" "$COMPLIANCE_TESTS"
run_test_category "Edge" "$EDGE_TESTS"
run_test_category "Performance" "$PERFORMANCE_TESTS"

# Create a list of all categorized tests
ALL_CATEGORIZED="$CORE_TESTS $FILEIO_TESTS $SCRIPTING_TESTS $PROCESS_TESTS $SHELL_TESTS $FEATURES_TESTS $COMPLIANCE_TESTS $EDGE_TESTS $PERFORMANCE_TESTS"

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