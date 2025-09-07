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
echo ""

# Define test categories and their tests (using simple variables instead of associative arrays)
CORE_TESTS="test_builtin_commands test_environment_vars test_command_line_options"
FILEIO_TESTS="test_file_operations test_redirections"
SCRIPTING_TESTS="test_scripting test_control_structures test_quoting_expansions"
PROCESS_TESTS="test_process_management test_pipeline test_job_control test_and_or"
SHELL_TESTS="test_alias test_cd test_cd_edges test_export"
FEATURES_TESTS="test_login_shell"
COMPLIANCE_TESTS="test_posix_compliance test_globbing"
EDGE_TESTS="test_error_handling test_error_edge_cases test_misc_edges"
PERFORMANCE_TESTS="test_performance"

run_test_category() {
    category=$1
    tests=$2
    echo "${YELLOW}=== $category Tests ===${NC}"
    
    for test_name in $tests; do
        test_file="$SHELL_TESTS_DIR/${test_name}.sh"
        if [ -f "$test_file" ]; then
            TOTAL=$((TOTAL+1))
            printf "  %-30s " "$test_name:"
            
            # Capture both output and exit code
            output=$(sh "$test_file" 2>&1)
            exit_code=$?
            
            if [ $exit_code -eq 0 ]; then
                echo "${GREEN}PASS${NC}"
                PASS=$((PASS+1))
            else
                echo "${RED}FAIL${NC}"
                echo "    Output: $output"
                FAIL=$((FAIL+1))
            fi
            
            # Check for warnings in output
            if echo "$output" | grep -q "WARNING"; then
                WARNINGS=$((WARNINGS+1))
                echo "    ${YELLOW}$(echo "$output" | grep "WARNING")${NC}"
            fi
        else
            echo "  ${YELLOW}SKIP${NC} $test_name (file not found)"
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

# Also run any remaining test files not categorized
echo "${YELLOW}=== Additional Tests ===${NC}"
for test in "$SHELL_TESTS_DIR"/test_*.sh; do
    test_name=$(basename "$test" .sh)
    found=false
    
    for categorized_test in $ALL_CATEGORIZED; do
        if [ "$categorized_test" = "$test_name" ]; then
            found=true
            break
        fi
    done
    
    if [ "$found" = false ]; then
        TOTAL=$((TOTAL+1))
        printf "  %-30s " "$test_name:"
        
        output=$(sh "$test" 2>&1)
        exit_code=$?
        
        if [ $exit_code -eq 0 ]; then
            echo "${GREEN}PASS${NC}"
            PASS=$((PASS+1))
        else
            echo "${RED}FAIL${NC}"
            echo "    Output: $output"
            FAIL=$((FAIL+1))
        fi
        
        if echo "$output" | grep -q "WARNING"; then
            WARNINGS=$((WARNINGS+1))
            echo "    ${YELLOW}$(echo "$output" | grep "WARNING")${NC}"
        fi
    fi
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