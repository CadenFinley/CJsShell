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

    if [ -n "$remaining" ]; then
        echo "FAIL: jobs output should be empty after disown"
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


if ! test_background_persists; then
    status=1
fi

if ! test_huponexit_kills_jobs; then
    status=1
fi

if ! test_disown_removes_job; then
    status=1
fi

exit $status
