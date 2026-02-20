#!/usr/bin/env sh
# Run all shell tests for cjsh
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

if [ -n "$1" ]; then
    CJSH_CANDIDATE="$1"
    shift
else
    CJSH_CANDIDATE="${CJSH:-$SCRIPT_DIR/../build/cjsh}"
fi

if [ "${CJSH_CANDIDATE#/}" = "$CJSH_CANDIDATE" ]; then
    CJSH_CANDIDATE="$(pwd)/$CJSH_CANDIDATE"
fi

CJSH_DIR=$(cd "$(dirname "$CJSH_CANDIDATE")" && pwd)
CJSH_BASENAME=$(basename "$CJSH_CANDIDATE")
CJSH="$CJSH_DIR/$CJSH_BASENAME"
export CJSH
SHELL_TESTS_DIR="$SCRIPT_DIR/shell"
DEFAULT_ISOCLINE_TEST_BINARY="$SCRIPT_DIR/../build/isocline_behavior_tests"
ISOCLINE_TEST_BINARY="${ISOCLINE_TEST_BINARY:-$DEFAULT_ISOCLINE_TEST_BINARY}"
DEFAULT_COMPLETION_TEST_BINARY="$SCRIPT_DIR/../build/completion_tests"
COMPLETION_TEST_BINARY="${COMPLETION_TEST_BINARY:-$DEFAULT_COMPLETION_TEST_BINARY}"
DEFAULT_SYNTAX_HIGHLIGHTING_TEST_BINARY="$SCRIPT_DIR/../build/syntax_highlighting_tests"
SYNTAX_HIGHLIGHTING_TEST_BINARY="${SYNTAX_HIGHLIGHTING_TEST_BINARY:-$DEFAULT_SYNTAX_HIGHLIGHTING_TEST_BINARY}"


if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ] || [ -n "$CONTINUOUS_INTEGRATION" ]; then
    export CJSH_CI_MODE="true"
fi


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' 

TOTAL_FILES=0
FILES_PASS=0
FILES_FAIL=0
WARNINGS=0


TOTAL_TESTS=0
TESTS_PASS=0
TESTS_FAIL=0
ISOCLINE_TEST_RESULT=0
COMPLETION_TEST_RESULT=0
SYNTAX_HIGHLIGHTING_TEST_RESULT=0


if [ ! -x "$CJSH" ]; then
    echo "${RED}Error: cjsh binary not found at $CJSH${NC}"
    echo "Please configure and build cjsh with CMake first (for example: 'cmake -S . -B build && cmake --build build')."
    exit 1
fi

echo "Using cjsh binary: $CJSH"

# echo "Checking for existing cjsh processes..."
# EXISTING_CJSH=$(pgrep -f "cjsh" 2>/dev/null || true)
# if [ -n "$EXISTING_CJSH" ]; then
#     echo "${RED}Error: Found running cjsh process(es): $EXISTING_CJSH${NC}"
#     echo "Please terminate any running cjsh instances before running tests."
#     echo "You can use: pkill -f cjsh"
#     exit 1
# else
#     echo "No existing cjsh processes found."
# fi

# echo "${BLUE}Running CJ's Shell Test Suite${NC}"
# echo "${BLUE}=============================${NC}"
# echo "Testing binary: $CJSH"
# echo "Includes comprehensive POSIX compliance tests"
# echo "This can take a second to complete. Hold tight."
# echo ""


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
        
        output=$(CJSH="$CJSH" CJSH_PATH="$CJSH" SHELL_TO_TEST="$CJSH" sh "$test_file" "$CJSH" 2>&1)
        exit_code=$?
        clean_output=$(printf "%s\n" "$output" | awk '{gsub(/\033\[[0-9;]*[A-Za-z]/, ""); print}')
        
        
        subtests_passed=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])PASS([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_failed=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])FAIL([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_total=$((subtests_passed + subtests_failed))
        
        
        TOTAL_TESTS=$((TOTAL_TESTS + subtests_total))
        TESTS_PASS=$((TESTS_PASS + subtests_passed))
        TESTS_FAIL=$((TESTS_FAIL + subtests_failed))
        
        if [ $exit_code -eq 0 ]; then
            echo "${GREEN}PASS${NC} (${subtests_passed}/${subtests_total})"
            FILES_PASS=$((FILES_PASS+1))
        else
            echo "${RED}FAIL${NC} (${subtests_passed}/${subtests_total}, ${subtests_failed} failed)"
            
            fail_lines=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])FAIL([^A-Za-z0-9_]|$)/) {print}')
            if [ -n "$fail_lines" ]; then
                printf "%s\n" "$fail_lines" | while IFS= read -r line; do
                    echo "    ${RED}$line${NC}"
                done
            fi

            if [ -n "${CJSH_CI_MODE-}" ] || [ -n "${CI-}" ] || [ -n "${GITHUB_ACTIONS-}" ]; then
                echo "    ${YELLOW}Raw output bytes for $test_name:${NC}"
                if command -v python3 >/dev/null 2>&1; then
                    printf '%s' "$output" | python3 -c 'import sys; data=sys.stdin.buffer.read(); print(data); print(list(data))'
                else
                    printf '%s' "$output" | od -An -t u1
                fi
            fi
            FILES_FAIL=$((FILES_FAIL+1))
        fi
        
        
        
        if printf "%s\n" "$clean_output" | grep -q "WARNING"; then
            WARNINGS=$((WARNINGS+1))
            echo "    ${YELLOW}$(printf "%s\n" "$clean_output" | grep "WARNING")${NC}"
        fi
    else
        echo "${RED}FAIL${NC} (file not found)"
        FILES_FAIL=$((FILES_FAIL+1))
    fi
}


# echo "${YELLOW}=== Running All Shell Tests ===${NC}"
test_list=$(discover_test_files)
for test_name in $test_list; do
    run_test "$test_name"
done

echo ""
# echo "${BLUE}Test Summary${NC}"
# echo "${BLUE}============${NC}"
echo "Total individual tests: $TOTAL_TESTS"
echo "${GREEN}Passed: $TESTS_PASS${NC}"
if [ $TESTS_FAIL -gt 0 ]; then
    echo "${RED}Failed: $TESTS_FAIL${NC}"
fi
echo ""
echo "Test files: $TOTAL_FILES"
echo "${GREEN}Files passed: $FILES_PASS${NC}"
echo "${RED}Files failed: $FILES_FAIL${NC}"
if [ $WARNINGS -gt 0 ]; then
    echo "${YELLOW}Warnings: $WARNINGS${NC}"
fi


if [ $TOTAL_TESTS -gt 0 ]; then
    PASS_PERCENTAGE=$(awk "BEGIN {printf \"%.2f\", ($TESTS_PASS / $TOTAL_TESTS) * 100}")
    echo ""
    echo "Pass rate: ${PASS_PERCENTAGE}% ($TESTS_PASS/$TOTAL_TESTS executed tests)"
fi

echo ""
if [ -x "$ISOCLINE_TEST_BINARY" ]; then
    echo "Running isocline behavior tests: $ISOCLINE_TEST_BINARY"
    "$ISOCLINE_TEST_BINARY"
    ISOCLINE_TEST_RESULT=$?
    if [ $ISOCLINE_TEST_RESULT -eq 0 ]; then
        echo "${GREEN}Isocline behavior tests passed${NC}"
    else
        echo "${RED}Isocline behavior tests failed${NC}"
    fi
else
    echo "${YELLOW}Skipping isocline behavior tests${NC} (binary not found at $ISOCLINE_TEST_BINARY)"
fi

if [ -x "$COMPLETION_TEST_BINARY" ]; then
    echo "Running completion tests: $COMPLETION_TEST_BINARY"
    "$COMPLETION_TEST_BINARY"
    COMPLETION_TEST_RESULT=$?
    if [ $COMPLETION_TEST_RESULT -eq 0 ]; then
        echo "${GREEN}Completion tests passed${NC}"
    else
        echo "${RED}Completion tests failed${NC}"
    fi
else
    echo "${YELLOW}Skipping completion tests${NC} (binary not found at $COMPLETION_TEST_BINARY)"
fi

if [ -x "$SYNTAX_HIGHLIGHTING_TEST_BINARY" ]; then
    echo "Running syntax highlighting tests: $SYNTAX_HIGHLIGHTING_TEST_BINARY"
    "$SYNTAX_HIGHLIGHTING_TEST_BINARY"
    SYNTAX_HIGHLIGHTING_TEST_RESULT=$?
    if [ $SYNTAX_HIGHLIGHTING_TEST_RESULT -eq 0 ]; then
        echo "${GREEN}Syntax highlighting tests passed${NC}"
    else
        echo "${RED}Syntax highlighting tests failed${NC}"
    fi
else
    echo "${YELLOW}Skipping syntax highlighting tests${NC} (binary not found at $SYNTAX_HIGHLIGHTING_TEST_BINARY)"
fi

OVERALL_STATUS=0
if [ $FILES_FAIL -ne 0 ]; then
    OVERALL_STATUS=$FILES_FAIL
fi
if [ $ISOCLINE_TEST_RESULT -ne 0 ]; then
    if [ $OVERALL_STATUS -eq 0 ]; then
        OVERALL_STATUS=$ISOCLINE_TEST_RESULT
    else
        OVERALL_STATUS=$((OVERALL_STATUS + ISOCLINE_TEST_RESULT))
    fi
fi
if [ $COMPLETION_TEST_RESULT -ne 0 ]; then
    if [ $OVERALL_STATUS -eq 0 ]; then
        OVERALL_STATUS=$COMPLETION_TEST_RESULT
    else
        OVERALL_STATUS=$((OVERALL_STATUS + COMPLETION_TEST_RESULT))
    fi
fi
if [ $SYNTAX_HIGHLIGHTING_TEST_RESULT -ne 0 ]; then
    if [ $OVERALL_STATUS -eq 0 ]; then
        OVERALL_STATUS=$SYNTAX_HIGHLIGHTING_TEST_RESULT
    else
        OVERALL_STATUS=$((OVERALL_STATUS + SYNTAX_HIGHLIGHTING_TEST_RESULT))
    fi
fi

if [ $OVERALL_STATUS -eq 0 ]; then
    echo ""
    echo "${GREEN}All tests passed!${NC}"
else
    echo ""
    echo "${RED}Some tests failed. Please review the output above.${NC}"
fi

exit $OVERALL_STATUS
