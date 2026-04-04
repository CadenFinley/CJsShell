#!/usr/bin/env sh

# run_shell_tests.sh
#
# This file is part of cjsh, CJ's Shell
#
# MIT License
#
# Copyright (c) 2026 Caden Finley
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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
DEFAULT_ISOCLINE_PTY_DRIVER_BINARY="$SCRIPT_DIR/../build/isocline_pty_driver"
ISOCLINE_PTY_DRIVER_BINARY="${ISOCLINE_PTY_DRIVER_BINARY:-$DEFAULT_ISOCLINE_PTY_DRIVER_BINARY}"
ISOCLINE_PTY_TEST_SCRIPT="$SCRIPT_DIR/isocline/test_isocline_pty.py"
BUILD_SYSTEM_TEST_SCRIPT="$SCRIPT_DIR/build_system/test_build_system.py"


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
ISOCLINE_PTY_TEST_RESULT=0
BUILD_SYSTEM_TEST_RESULT=0


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

is_non_negative_integer() {
    case "$1" in
        ''|*[!0-9]*)
            return 1
            ;;
        *)
            return 0
            ;;
    esac
}

add_individual_counts() {
    add_total=$1
    add_pass=$2
    add_fail=$3

    if ! is_non_negative_integer "$add_total"; then
        add_total=0
    fi
    if ! is_non_negative_integer "$add_pass"; then
        add_pass=0
    fi
    if ! is_non_negative_integer "$add_fail"; then
        add_fail=0
    fi

    TOTAL_TESTS=$((TOTAL_TESTS + add_total))
    TESTS_PASS=$((TESTS_PASS + add_pass))
    TESTS_FAIL=$((TESTS_FAIL + add_fail))
}

parse_cstyle_test_counts() {
    suite_output=$1
    printf "%s\n" "$suite_output" | awk '
        $0 ~ /^All[[:space:]]+[0-9]+[[:space:]].*tests passed/ {
            count = split($0, fields, /[[:space:]]+/)
            if (count >= 2 && fields[2] ~ /^[0-9]+$/) {
                total = fields[2] + 0
                print total " " total " 0"
                found = 1
                exit
            }
        }
        $0 ~ /[0-9]+\/[0-9]+[[:space:]].*tests failed/ {
            count = split($0, fields, /[[:space:]]+/)
            for (i = 1; i <= count; ++i) {
                if (fields[i] ~ /^[0-9]+\/[0-9]+$/) {
                    split(fields[i], parts, "/")
                    failed = parts[1] + 0
                    total = parts[2] + 0
                    passed = total - failed
                    if (passed < 0) {
                        passed = 0
                    }
                    print total " " passed " " failed
                    found = 1
                    exit
                }
            }
        }
        END {
            if (!found) {
                print "0 0 0"
            }
        }
    '
}

parse_python_unittest_counts() {
    suite_output=$1
    suite_exit_code=$2

    suite_total=$(printf "%s\n" "$suite_output" | awk '
        /^Ran [0-9]+ tests?/ {
            print $2
            found = 1
            exit
        }
        END {
            if (!found) {
                print 0
            }
        }
    ')

    if ! is_non_negative_integer "$suite_total"; then
        printf "0 0 0"
        return
    fi

    if [ "$suite_total" -eq 0 ]; then
        printf "0 0 0"
        return
    fi

    if [ "$suite_exit_code" -eq 0 ]; then
        printf "%s %s 0" "$suite_total" "$suite_total"
        return
    fi

    suite_failed=$(printf "%s\n" "$suite_output" | awk '
        /FAILED \(/ {
            summary = $0
            sub(/^.*FAILED \(/, "", summary)
            sub(/\).*/, "", summary)
            count = split(summary, fields, ",")
            failed = 0
            for (i = 1; i <= count; ++i) {
                gsub(/^[[:space:]]+|[[:space:]]+$/, "", fields[i])
                split(fields[i], kv, "=")
                if (length(kv) == 2 && kv[2] ~ /^[0-9]+$/) {
                    failed += kv[2]
                }
            }
            print failed
            found = 1
            exit
        }
        END {
            if (!found) {
                print 0
            }
        }
    ')

    if ! is_non_negative_integer "$suite_failed"; then
        suite_failed=0
    fi
    if [ "$suite_failed" -eq 0 ]; then
        suite_failed=1
    fi

    suite_passed=$((suite_total - suite_failed))
    if [ "$suite_passed" -lt 0 ]; then
        suite_passed=0
    fi
    printf "%s %s %s" "$suite_total" "$suite_passed" "$suite_failed"
}

sanitize_terminal_output() {
    raw_text=$1
    printf "%s\n" "$raw_text" | awk '
        BEGIN {
            esc = sprintf("%c", 27)
            bel = sprintf("%c", 7)
            csi = esc "\\[[0-?]*[ -/]*[@-~]"
            osc_bel = esc "\\][^" bel "]*" bel
            osc_st = esc "\\][^" esc "]*" esc "\\\\"
        }
        {
            line = $0
            gsub(osc_bel, "", line)
            gsub(osc_st, "", line)
            gsub(csi, "", line)
            gsub(esc, "", line)
            gsub(bel, "", line)
            gsub(/[^[:print:]\t]/, "", line)
            print line
        }
    '
}

run_test() {
    test_name=$1
    test_file="$SHELL_TESTS_DIR/${test_name}.sh"
    TOTAL_FILES=$((TOTAL_FILES+1))
    printf "  %-50s " "$test_name:"
    
    if [ -f "$test_file" ]; then
        
        output=$(CJSH="$CJSH" CJSH_PATH="$CJSH" SHELL_TO_TEST="$CJSH" sh "$test_file" "$CJSH" </dev/null 2>&1)
        exit_code=$?
        clean_output=$(printf "%s\n" "$output" | awk '{gsub(/\033\[[0-9;]*[A-Za-z]/, ""); print}')
        
        
        subtests_passed=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])PASS([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_failed=$(printf "%s\n" "$clean_output" | awk 'match($0, /(^|[^A-Za-z0-9_])FAIL([^A-Za-z0-9_]|$)/) {count++} END {print count+0}')
        subtests_total=$((subtests_passed + subtests_failed))
        
        
        add_individual_counts "$subtests_total" "$subtests_passed" "$subtests_failed"
        
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
if [ -x "$ISOCLINE_TEST_BINARY" ]; then
    echo "Running isocline behavior tests: $ISOCLINE_TEST_BINARY"
    ISOCLINE_TEST_OUTPUT=$("$ISOCLINE_TEST_BINARY" 2>&1)
    ISOCLINE_TEST_RESULT=$?
    ISOCLINE_TEST_OUTPUT_CLEAN=$(sanitize_terminal_output "$ISOCLINE_TEST_OUTPUT")
    printf "%s\n" "$ISOCLINE_TEST_OUTPUT_CLEAN"
    ISOCLINE_TEST_COUNTS=$(parse_cstyle_test_counts "$ISOCLINE_TEST_OUTPUT_CLEAN")
    set -- $ISOCLINE_TEST_COUNTS
    add_individual_counts "$1" "$2" "$3"
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
    COMPLETION_TEST_OUTPUT=$("$COMPLETION_TEST_BINARY" 2>&1)
    COMPLETION_TEST_RESULT=$?
    COMPLETION_TEST_OUTPUT_CLEAN=$(sanitize_terminal_output "$COMPLETION_TEST_OUTPUT")
    printf "%s\n" "$COMPLETION_TEST_OUTPUT_CLEAN"
    COMPLETION_TEST_COUNTS=$(parse_cstyle_test_counts "$COMPLETION_TEST_OUTPUT_CLEAN")
    set -- $COMPLETION_TEST_COUNTS
    add_individual_counts "$1" "$2" "$3"
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
    SYNTAX_HIGHLIGHTING_TEST_OUTPUT=$("$SYNTAX_HIGHLIGHTING_TEST_BINARY" 2>&1)
    SYNTAX_HIGHLIGHTING_TEST_RESULT=$?
    SYNTAX_HIGHLIGHTING_TEST_OUTPUT_CLEAN=$(sanitize_terminal_output "$SYNTAX_HIGHLIGHTING_TEST_OUTPUT")
    printf "%s\n" "$SYNTAX_HIGHLIGHTING_TEST_OUTPUT_CLEAN"
    SYNTAX_HIGHLIGHTING_TEST_COUNTS=$(parse_cstyle_test_counts "$SYNTAX_HIGHLIGHTING_TEST_OUTPUT_CLEAN")
    set -- $SYNTAX_HIGHLIGHTING_TEST_COUNTS
    add_individual_counts "$1" "$2" "$3"
    if [ $SYNTAX_HIGHLIGHTING_TEST_RESULT -eq 0 ]; then
        echo "${GREEN}Syntax highlighting tests passed${NC}"
    else
        echo "${RED}Syntax highlighting tests failed${NC}"
    fi
else
    echo "${YELLOW}Skipping syntax highlighting tests${NC} (binary not found at $SYNTAX_HIGHLIGHTING_TEST_BINARY)"
fi

if [ -x "$ISOCLINE_PTY_DRIVER_BINARY" ] && [ -f "$ISOCLINE_PTY_TEST_SCRIPT" ]; then
    if command -v python3 >/dev/null 2>&1; then
        echo "Running isocline PTY integration tests: $ISOCLINE_PTY_TEST_SCRIPT"
        ISOCLINE_PTY_TEST_OUTPUT=$(python3 "$ISOCLINE_PTY_TEST_SCRIPT" "$ISOCLINE_PTY_DRIVER_BINARY" 2>&1)
        ISOCLINE_PTY_TEST_RESULT=$?
        ISOCLINE_PTY_TEST_OUTPUT_CLEAN=$(sanitize_terminal_output "$ISOCLINE_PTY_TEST_OUTPUT")
        printf "%s\n" "$ISOCLINE_PTY_TEST_OUTPUT_CLEAN"
        ISOCLINE_PTY_COUNTS=$(parse_cstyle_test_counts "$ISOCLINE_PTY_TEST_OUTPUT_CLEAN")
        set -- $ISOCLINE_PTY_COUNTS
        if [ "$1" -eq 0 ] && [ "$2" -eq 0 ] && [ "$3" -eq 0 ]; then
            if [ $ISOCLINE_PTY_TEST_RESULT -eq 0 ]; then
                add_individual_counts 1 1 0
            else
                add_individual_counts 1 0 1
            fi
        else
            add_individual_counts "$1" "$2" "$3"
        fi
        if [ $ISOCLINE_PTY_TEST_RESULT -eq 0 ]; then
            echo "${GREEN}Isocline PTY integration tests passed${NC}"
        else
            echo "${RED}Isocline PTY integration tests failed${NC}"
        fi
    else
        echo "${YELLOW}Skipping isocline PTY integration tests${NC} (python3 not found)"
    fi
else
    echo "${YELLOW}Skipping isocline PTY integration tests${NC} (driver not found at $ISOCLINE_PTY_DRIVER_BINARY)"
fi

if [ -f "$BUILD_SYSTEM_TEST_SCRIPT" ]; then
    if command -v python3 >/dev/null 2>&1 && command -v cmake >/dev/null 2>&1; then
        echo "Running build system configuration tests: $BUILD_SYSTEM_TEST_SCRIPT"
        BUILD_SYSTEM_TEST_OUTPUT=$(python3 "$BUILD_SYSTEM_TEST_SCRIPT" "$SCRIPT_DIR/.." 2>&1)
        BUILD_SYSTEM_TEST_RESULT=$?
        BUILD_SYSTEM_TEST_OUTPUT_CLEAN=$(sanitize_terminal_output "$BUILD_SYSTEM_TEST_OUTPUT")
        printf "%s\n" "$BUILD_SYSTEM_TEST_OUTPUT_CLEAN"
        BUILD_SYSTEM_TEST_COUNTS=$(parse_python_unittest_counts "$BUILD_SYSTEM_TEST_OUTPUT_CLEAN" "$BUILD_SYSTEM_TEST_RESULT")
        set -- $BUILD_SYSTEM_TEST_COUNTS
        add_individual_counts "$1" "$2" "$3"
        if [ $BUILD_SYSTEM_TEST_RESULT -eq 0 ]; then
            echo "${GREEN}Build system configuration tests passed${NC}"
        else
            echo "${RED}Build system configuration tests failed${NC}"
        fi
    else
        echo "${YELLOW}Skipping build system configuration tests${NC} (python3 or cmake not found)"
    fi
else
    echo "${YELLOW}Skipping build system configuration tests${NC} (script not found at $BUILD_SYSTEM_TEST_SCRIPT)"
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
if [ $ISOCLINE_PTY_TEST_RESULT -ne 0 ]; then
    if [ $OVERALL_STATUS -eq 0 ]; then
        OVERALL_STATUS=$ISOCLINE_PTY_TEST_RESULT
    else
        OVERALL_STATUS=$((OVERALL_STATUS + ISOCLINE_PTY_TEST_RESULT))
    fi
fi
if [ $BUILD_SYSTEM_TEST_RESULT -ne 0 ]; then
    if [ $OVERALL_STATUS -eq 0 ]; then
        OVERALL_STATUS=$BUILD_SYSTEM_TEST_RESULT
    else
        OVERALL_STATUS=$((OVERALL_STATUS + BUILD_SYSTEM_TEST_RESULT))
    fi
fi

echo ""
echo "Total individual tests: $TOTAL_TESTS"
echo "${GREEN}Passed: $TESTS_PASS${NC}"
echo "${RED}Failed: $TESTS_FAIL${NC}"
echo ""
echo "Test files: $TOTAL_FILES"
echo "${GREEN}Files passed: $FILES_PASS${NC}"
echo "${RED}Files failed: $FILES_FAIL${NC}"
if [ $WARNINGS -gt 0 ]; then
    echo "${YELLOW}Warnings: $WARNINGS${NC}"
fi

if [ $TOTAL_TESTS -gt 0 ]; then
    PASS_PERCENTAGE=$(awk "BEGIN {printf \"%.2f\", ($TESTS_PASS / $TOTAL_TESTS) * 100}")
    echo "Pass rate: ${PASS_PERCENTAGE}% ($TESTS_PASS/$TOTAL_TESTS executed tests)"
fi

if [ $OVERALL_STATUS -eq 0 ]; then
    echo ""
    echo "${GREEN}All tests passed!${NC}"
else
    echo ""
    echo "${RED}Some tests failed. Please review the output above.${NC}"
fi

exit $OVERALL_STATUS
