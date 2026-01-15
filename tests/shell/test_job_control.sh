#!/usr/bin/env bash

if [ -n "$CJSH" ]; then
    CJSH_PATH="$CJSH"
else
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

status=0

log() {
    echo "$1"
}

cleanup_pid() {
    local pid="$1"
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
    fi
}

test_background_persists() {
    log "Test: background job survives without huponexit"
    local pid_file
    pid_file="$(mktemp /tmp/cjsh_job_pid.XXXXXX)"

    "$CJSH_PATH" -c "sleep 5 & echo \$! > $pid_file; sleep 0.2" >/dev/null 2>&1

    local pid
    pid="$(cat "$pid_file" 2>/dev/null)"
    rm -f "$pid_file"

    if [ -z "$pid" ]; then
        echo "FAIL: no PID recorded"
        return 1
    fi

    if kill -0 "$pid" 2>/dev/null; then
        cleanup_pid "$pid"
        echo "PASS"
        return 0
    else
        echo "FAIL: background PID $pid not running"
        return 1
    fi
}

test_huponexit_kills_jobs() {
    log "Test: set -o huponexit terminates jobs on exit"
    local pid_file
    pid_file="$(mktemp /tmp/cjsh_job_pid.XXXXXX)"

    "$CJSH_PATH" -c "set -o huponexit; sleep 5 & echo \$! > $pid_file; sleep 0.2" >/dev/null 2>&1

    local pid
    pid="$(cat "$pid_file" 2>/dev/null)"
    rm -f "$pid_file"

    if [ -z "$pid" ]; then
        echo "FAIL: no PID recorded"
        return 1
    fi

    if kill -0 "$pid" 2>/dev/null; then
        cleanup_pid "$pid"
        echo "FAIL: PID $pid should have been terminated"
        return 1
    fi

    echo "PASS"
    return 0
}

test_disown_removes_job() {
    log "Test: disown detaches current job"
    local pid_file log_file
    pid_file="$(mktemp /tmp/cjsh_job_pid.XXXXXX)"
    log_file="$(mktemp /tmp/cjsh_jobs.XXXXXX)"

    "$CJSH_PATH" -c "sleep 5 & echo \$! > $pid_file; sleep 0.2; disown; jobs" >"$log_file" 2>&1

    local pid
    pid="$(cat "$pid_file" 2>/dev/null)"
    rm -f "$pid_file"

    if [ -z "$pid" ]; then
        rm -f "$log_file"
        echo "FAIL: no PID recorded"
        return 1
    fi

    local result=0
    local remaining
    remaining=$(grep -v '^\[' "$log_file" | sed '/^[[:space:]]*$/d')

    if [ "$remaining" != "No jobs" ]; then
        echo "FAIL: expected 'No jobs' after disown, got: $remaining"
        result=1
    elif ! kill -0 "$pid" 2>/dev/null; then
        echo "FAIL: disowned PID $pid was not running"
        result=1
    else
        echo "PASS"
    fi

    rm -f "$log_file"
    cleanup_pid "$pid"
    return $result
}

jobs_reports_when_empty() {
    log "Test: jobs reports when no jobs exist"
    local output
    output=$("$CJSH_PATH" -c "jobs" 2>&1)

    if echo "$output" | grep -q "No jobs"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected 'No jobs' message, got: $output"
    return 1
}

jobs_p_option_stays_silent_when_empty() {
    log "Test: jobs -p stays silent when empty"
    local output
    output=$("$CJSH_PATH" -c "jobs -p" 2>&1)

    if [ -n "$output" ]; then
        echo "FAIL: jobs -p should produce no output when empty, got: $output"
        return 1
    fi

    echo "PASS"
    return 0
}

fg_command_name_resolves_job() {
    log "Test: fg resolves job by command name"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 1 & fg sleep" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: fg sleep via command name failed (exit $exit_code): $output"
        return 1
    fi

    echo "PASS"
    return 0
}

fg_command_name_requires_disambiguation() {
    log "Test: fg requires job id when command name is ambiguous"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 5 & pid1=\$!; sleep 5 & pid2=\$!; fg sleep; fg_status=\$?; kill \$pid1 \$pid2 2>/dev/null; wait \$pid1 \$pid2 2>/dev/null; exit \$fg_status" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "multiple jobs match command"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: fg sleep should require disambiguation (exit $exit_code): $output"
    return 1
}

kill_command_name_resolves_job() {
    log "Test: kill resolves job by command name"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 5 & pid=\$!; kill sleep; kill_status=\$?; kill \$pid 2>/dev/null; wait \$pid 2>/dev/null; exit \$kill_status" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: kill sleep via command name failed (exit $exit_code): $output"
    return 1
}

kill_command_name_requires_disambiguation() {
    log "Test: kill requires job id when command name is ambiguous"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 5 & pid1=\$!; sleep 5 & pid2=\$!; kill sleep; kill_status=\$?; kill \$pid1 \$pid2 2>/dev/null; wait \$pid1 \$pid2 2>/dev/null; exit \$kill_status" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "multiple jobs match command"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: kill sleep should require disambiguation (exit $exit_code): $output"
    return 1
}

background_failure_prints_exit_code() {
    log "Test: background command failure reports exit status"
    local output
    output=$("$CJSH_PATH" -i -c "slepp 0.01 & sleep 0.2" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code"
        return 1
    fi

    if echo "$output" | grep -q "Exit 127"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected Exit 127 notification, got: $output"
    return 1
}

if ! test_background_persists; then
    status=1
fi

if ! test_huponexit_kills_jobs; then
    status=1
fi

if ! test_disown_removes_job; then
    status=1
fi

if ! jobs_reports_when_empty; then
    status=1
fi

if ! jobs_p_option_stays_silent_when_empty; then
    status=1
fi

if ! fg_command_name_resolves_job; then
    status=1
fi

if ! fg_command_name_requires_disambiguation; then
    status=1
fi

if ! kill_command_name_resolves_job; then
    status=1
fi

if ! kill_command_name_requires_disambiguation; then
    status=1
fi

if ! background_failure_prints_exit_code; then
    status=1
fi

exit $status
