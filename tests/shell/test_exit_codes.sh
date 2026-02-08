#!/usr/bin/env sh

#

TOTAL=0
PASSED=0
FAILED=0

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
DEFAULT_SHELL="$SCRIPT_DIR/../../build/cjsh"

if [ -n "$1" ]; then
    SHELL_TO_TEST="$1"
elif [ -z "$SHELL_TO_TEST" ]; then
    if [ -n "$CJSH" ]; then
        SHELL_TO_TEST="$CJSH"
    else
        SHELL_TO_TEST="$DEFAULT_SHELL"
    fi
fi

if [ "${SHELL_TO_TEST#/}" = "$SHELL_TO_TEST" ]; then
    SHELL_TO_TEST="$(pwd)/$SHELL_TO_TEST"
fi


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_test() {
    TOTAL=$((TOTAL + 1))
    printf "Test %03d: %s... " "$TOTAL" "$1"
}

pass() {
    PASSED=$((PASSED + 1))
    printf "${GREEN}PASS${NC}\n"
}

fail() {
    FAILED=$((FAILED + 1))
    printf "${RED}FAIL${NC} - %s\n" "$1"
}

create_temp_file() {
    local content="$1"
    local permissions="$2"
    local temp_file
    temp_file=$(mktemp /tmp/cjsh_test_XXXXXX)
    printf "%b\n" "$content" > "$temp_file"
    if [ -n "$permissions" ]; then
        chmod "$permissions" "$temp_file"
    fi
    echo "$temp_file"
}

cleanup_temp_files() {
    for file in "$@"; do
        if [ -f "$file" ]; then
            rm -f "$file" 2>/dev/null
        fi
    done
}

if [ ! -x "$SHELL_TO_TEST" ]; then
    echo "Error: Shell '$SHELL_TO_TEST' not found or not executable"
    echo "Usage: $0 [path_to_shell]"
    exit 1
fi

echo "Testing Exit Codes for: $SHELL_TO_TEST"
echo "======================================"
echo "Testing comprehensive exit code behavior according to POSIX standards"
echo ""


log_test "Exit code 0 - Success (explicit exit 0)"
"$SHELL_TO_TEST" -c "exit 0" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Expected exit code 0, got $?"
fi

log_test "Exit code 0 - Success (implicit, successful command)"
"$SHELL_TO_TEST" -c "true" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Expected exit code 0 from 'true' command, got $?"
fi

log_test "Exit code 0 - Success (empty command)"
"$SHELL_TO_TEST" -c "" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Expected exit code 0 from empty command, got $?"
fi


log_test "Exit code 1 - General error (explicit exit 1)"
"$SHELL_TO_TEST" -c "exit 1" 2>/dev/null
if [ $? -eq 1 ]; then
    pass
else
    fail "Expected exit code 1, got $?"
fi

log_test "Exit code 1 - General error (false command)"
"$SHELL_TO_TEST" -c "false" 2>/dev/null
if [ $? -eq 1 ]; then
    pass
else
    fail "Expected exit code 1 from 'false' command, got $?"
fi

log_test "Exit code 1 - General error (test command failure)"
"$SHELL_TO_TEST" -c "test 1 -eq 2" 2>/dev/null
if [ $? -eq 1 ]; then
    pass
else
    fail "Expected exit code 1 from failed test, got $?"
fi


log_test "Exit code 2 - Syntax error (unclosed quote)"
"$SHELL_TO_TEST" -c "echo 'unclosed quote" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 2 ]; then
    pass
else
    fail "Expected exit code 2 for syntax error, got $exit_code"
fi

log_test "Exit code 2 - Syntax error (invalid redirection)"
"$SHELL_TO_TEST" -c "echo hello >" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 2 ]; then
    pass
else
    fail "Expected exit code 2 for invalid redirection, got $exit_code"
fi

log_test "Exit code 2 - Builtin misuse (cd with too many arguments)"
"$SHELL_TO_TEST" -c "cd /tmp /usr /var" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 1 ] || [ $exit_code -eq 2 ]; then
    pass
else
    fail "Expected exit code 1 or 2 for cd misuse, got $exit_code"
fi


non_executable_script=$(create_temp_file "#!/bin/sh\necho 'Hello World'" 644)

log_test "Exit code 126 - Permission denied (non-executable file)"
"$SHELL_TO_TEST" -c "$non_executable_script" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 126 ]; then
    pass
else
    if [ $exit_code -eq 127 ]; then
        fail "System returned 127 instead of 126 for non-executable file"
    else
        fail "Expected exit code 126 for non-executable file, got $exit_code"
    fi
fi

test_dir=$(mktemp -d /tmp/cjsh_test_dir_XXXXXX)

log_test "Exit code 126 - Cannot execute directory"
"$SHELL_TO_TEST" -c "$test_dir" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 126 ]; then
    pass
else
    if [ $exit_code -eq 127 ]; then
        fail "System returned 127 instead of 126 for directory execution"
    else
        fail "Expected exit code 126 for directory execution, got $exit_code"
    fi
fi


log_test "Exit code 127 - Command not found (nonexistent command)"
"$SHELL_TO_TEST" -c "nonexistent_command_12345" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 127 ]; then
    pass
else
    fail "Expected exit code 127 for nonexistent command, got $exit_code"
fi

log_test "Exit code 127 - Command not found (invalid path)"
"$SHELL_TO_TEST" -c "/nonexistent/path/to/command" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 127 ]; then
    pass
else
    fail "Expected exit code 127 for invalid path, got $exit_code"
fi


log_test "Exit code 128 - Invalid exit argument (non-numeric)"
"$SHELL_TO_TEST" -c "exit abc" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 128 ] || [ $exit_code -eq 2 ]; then
    pass
else
    fail "Expected exit code 128 or 2 for invalid exit argument, got $exit_code"
fi

log_test "Exit code 128 - Invalid exit argument (too many args)"
"$SHELL_TO_TEST" -c "exit 1 2 3" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 128 ] || [ $exit_code -eq 2 ] || [ $exit_code -eq 1 ]; then
    pass
else
    fail "Expected exit code 128, 2, or 1 for too many exit args, got $exit_code"
fi


log_test "Exit code 130 - SIGINT (Control-C)"
sleep_script=$(create_temp_file "#!/bin/sh\nsleep 10" 755)
(
    "$SHELL_TO_TEST" -c "$sleep_script" &
    pid=$!
    sleep 0.1
    kill -INT $pid 2>/dev/null
    wait $pid 2>/dev/null
) 2>/dev/null
exit_code=$?
if [ $exit_code -eq 130 ]; then
    pass
else
    if [ $exit_code -ge 128 ] && [ $exit_code -le 143 ]; then
        fail "Got signal-related exit code $exit_code instead of 130"
    else
        fail "Expected exit code 130 for SIGINT, got $exit_code"
    fi
fi

log_test "Exit code 143 - SIGTERM"
sleep_script2=$(create_temp_file "#!/bin/sh\nsleep 10" 755)
(
    "$SHELL_TO_TEST" -c "$sleep_script2" &
    pid=$!
    sleep 0.1
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
) 2>/dev/null
exit_code=$?
if [ $exit_code -eq 143 ]; then
    pass
else
    if [ $exit_code -ge 128 ] && [ $exit_code -le 160 ]; then
        fail "Got signal-related exit code $exit_code instead of 143"
    else
        fail "Expected exit code 143 for SIGTERM, got $exit_code"
    fi
fi

log_test "Exit code 137 - SIGKILL"
sleep_script3=$(create_temp_file "#!/bin/sh\nsleep 10" 755)
(
    "$SHELL_TO_TEST" -c "$sleep_script3" &
    pid=$!
    sleep 0.1
    kill -KILL $pid 2>/dev/null
    wait $pid 2>/dev/null
) 2>/dev/null
exit_code=$?
if [ $exit_code -eq 137 ]; then
    pass
else
    if [ $exit_code -ge 128 ] && [ $exit_code -le 160 ]; then
        fail "Got signal-related exit code $exit_code instead of 137"
    else
        fail "Expected exit code 137 for SIGKILL, got $exit_code"
    fi
fi


log_test "Exit code 255 - Exit status out of range (exit -1)"
"$SHELL_TO_TEST" -c "exit -1" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 255 ]; then
    pass
else
    fail "Expected exit code 255 for exit -1, got $exit_code"
fi

log_test "Exit code modulo 256 - Large exit code (exit 300)"
"$SHELL_TO_TEST" -c "exit 300" 2>/dev/null
exit_code=$?
expected=44  # 300 % 256 = 44
if [ $exit_code -eq $expected ]; then
    pass
else
    fail "Expected exit code $expected for exit 300 (300 % 256), got $exit_code"
fi

log_test "Exit code modulo 256 - Very large exit code (exit 1000)"
"$SHELL_TO_TEST" -c "exit 1000" 2>/dev/null
exit_code=$?
expected=232  # 1000 % 256 = 232
if [ $exit_code -eq $expected ]; then
    pass
else
    fail "Expected exit code $expected for exit 1000 (1000 % 256), got $exit_code"
fi


log_test "Last command exit code propagation"
"$SHELL_TO_TEST" -c "false; echo 'after false'" >/dev/null 2>&1
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass
else
    fail "Expected exit code 0 (last command success), got $exit_code"
fi

log_test "Pipeline exit code (last command in pipeline)"
"$SHELL_TO_TEST" -c "echo 'hello' | false" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 1 ]; then
    pass
else
    fail "Expected exit code 1 from pipeline with false, got $exit_code"
fi

log_test "Subshell exit code propagation"
"$SHELL_TO_TEST" -c "(exit 42)" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 42 ]; then
    pass
else
    fail "Expected exit code 42 from subshell, got $exit_code"
fi

log_test "Command substitution does not affect main exit code"
"$SHELL_TO_TEST" -c "result=\$(false); true" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 0 ]; then
    pass
else
    fail "Expected exit code 0 (command substitution should not affect main), got $exit_code"
fi


script_exit_0=$(create_temp_file "#!/bin/sh\nexit 0" 755)
log_test "Script file exit code 0"
"$SHELL_TO_TEST" "$script_exit_0" 2>/dev/null
if [ $? -eq 0 ]; then
    pass
else
    fail "Expected exit code 0 from script file, got $?"
fi

script_exit_42=$(create_temp_file "#!/bin/sh\nexit 42" 755)
log_test "Script file exit code 42"
"$SHELL_TO_TEST" "$script_exit_42" 2>/dev/null
if [ $? -eq 42 ]; then
    pass
else
    fail "Expected exit code 42 from script file, got $?"
fi

script_syntax_error=$(create_temp_file "#!/bin/sh\necho 'unclosed quote" 755)
log_test "Script file syntax error"
"$SHELL_TO_TEST" "$script_syntax_error" 2>/dev/null
exit_code=$?
if [ $exit_code -eq 2 ]; then
    pass
else
    fail "Expected exit code 2 from script with syntax error, got $exit_code"
fi


cleanup_temp_files "$non_executable_script" "$sleep_script" "$sleep_script2" "$sleep_script3" \
                  "$script_exit_0" "$script_exit_42" "$script_syntax_error"
if [ -d "$test_dir" ]; then
    rmdir "$test_dir" 2>/dev/null
fi

echo ""
echo "================================================================"
echo "Exit Code Test Results:"
echo "  Total tests: $TOTAL"
echo "  Passed:      ${GREEN}$PASSED${NC}"
echo "  Failed:      ${RED}$FAILED${NC}"
if [ $FAILED -eq 0 ]; then
    echo "  ${GREEN}All exit code tests passed!${NC}"
    exit 0
else
    echo "  ${RED}Some exit code tests failed.${NC}"
    exit 1
fi
