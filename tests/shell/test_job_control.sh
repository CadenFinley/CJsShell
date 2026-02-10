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

wait_for_pid_file() {
    local pid_file="$1"
    local retries=0
    local pid=""

    while [ $retries -lt 60 ]; do
        if [ -s "$pid_file" ]; then
            pid="$(cat "$pid_file" 2>/dev/null)"
            if [ -n "$pid" ]; then
                echo "$pid"
                return 0
            fi
        fi
        sleep 0.05
        retries=$((retries + 1))
    done

    return 1
}

wait_for_process_exit() {
    local pid="$1"
    local retries=0

    while kill -0 "$pid" 2>/dev/null; do
        if [ $retries -ge 200 ]; then
            return 1
        fi
        sleep 0.05
        retries=$((retries + 1))
    done

    return 0
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

jobname_updates_jobs_output() {
    log "Test: jobname changes display in jobs"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 2 & pid=\$!; if ! jobname \$pid renamed-job; then exit \$?; fi; jobs; kill \$pid 2>/dev/null; wait \$pid 2>/dev/null || true" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: jobname command failed (exit $exit_code): $output"
        return 1
    fi

    if echo "$output" | grep -q "Running[[:space:]]\+renamed-job"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected jobs output to include renamed job, got: $output"
    return 1
}

jobname_affects_command_matching() {
    log "Test: jobname affects fg command resolution"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 0.2 & pid=\$!; if ! jobname \$pid special-name; then exit \$?; fi; fg special-name" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: fg special-name failed (exit $exit_code): $output"
    return 1
}

jobname_rejects_empty_name() {
    log "Test: jobname rejects empty rename"
    local output
    output=$("$CJSH_PATH" -i -c "sleep 1 & pid=\$!; jobname \$pid ' '; status=\$?; kill \$pid 2>/dev/null; wait \$pid 2>/dev/null || true; exit \$status" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 1 ] && echo "$output" | grep -qi "cannot be empty"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: jobname should reject empty names (exit $exit_code): $output"
    return 1
}

auto_background_on_stop() {
    log "Test: &^ auto-backgrounds on SIGTSTP"
    local pid_file output_file
    pid_file="$(mktemp /tmp/cjsh_autobg_pid.XXXXXX)"
    output_file="$(mktemp /tmp/cjsh_autobg_out.XXXXXX)"

    "$CJSH_PATH" -i -c "sh -c 'echo \$\$ > $pid_file; sleep 5' &^; jobs; kill -9 %1 2>/dev/null; wait %1 2>/dev/null || true" >"$output_file" 2>&1 &
    local cjsh_pid=$!

    local target_pid
    target_pid="$(wait_for_pid_file "$pid_file")"

    if [ -z "$target_pid" ]; then
        kill "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file"
        echo "FAIL: no foreground PID recorded"
        return 1
    fi

    kill -TSTP "-$target_pid" 2>/dev/null || kill -TSTP "$target_pid" 2>/dev/null

    if ! wait_for_process_exit "$cjsh_pid"; then
        kill -CONT "$cjsh_pid" 2>/dev/null || true
        kill -9 "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file"
        echo "FAIL: cjsh did not exit after SIGTSTP"
        return 1
    fi

    wait "$cjsh_pid"
    local exit_code=$?
    local output
    output="$(cat "$output_file" 2>/dev/null)"

    rm -f "$pid_file" "$output_file"

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code: $output"
        return 1
    fi

    if echo "$output" | grep -q "Stopped"; then
        echo "FAIL: job reported stopped instead of running: $output"
        return 1
    fi

    if echo "$output" | grep -q "Running"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected Running job output, got: $output"
    return 1
}

auto_background_no_caret_command() {
    log "Test: &^ does not spawn caret command"
    local output
    output=$("$CJSH_PATH" -c "sleep 0.05 &^; sleep 0.05 &^ && echo ok" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code: $output"
        return 1
    fi

    if echo "$output" | grep -q "cjsh: \^"; then
        echo "FAIL: caret command spawned: $output"
        return 1
    fi

    if echo "$output" | grep -q "command not found"; then
        echo "FAIL: unexpected command not found: $output"
        return 1
    fi

    if echo "$output" | grep -q "ok"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected ok output, got: $output"
    return 1
}

auto_background_literal_amp_caret() {
    log "Test: quoted &^ stays literal"
    local output
    output=$("$CJSH_PATH" -c "echo '&^'; echo \\&^" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code: $output"
        return 1
    fi

    if echo "$output" | grep -q "command not found"; then
        echo "FAIL: unexpected command not found: $output"
        return 1
    fi

    local count
    count=$(echo "$output" | grep -c "&^")
    if [ "$count" -ge 2 ]; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected literal &^ output, got: $output"
    return 1
}

auto_background_ignores_sigstop() {
    log "Test: &^ does not auto-background on SIGSTOP"
    local pid_file output_file
    pid_file="$(mktemp /tmp/cjsh_autobg_pid.XXXXXX)"
    output_file="$(mktemp /tmp/cjsh_autobg_out.XXXXXX)"

    "$CJSH_PATH" -i -c "sh -c 'echo \$\$ > $pid_file; sleep 5' &^; jobs; kill -9 %1 2>/dev/null; wait %1 2>/dev/null || true" >"$output_file" 2>&1 &
    local cjsh_pid=$!

    local target_pid
    target_pid="$(wait_for_pid_file "$pid_file")"
    if [ -z "$target_pid" ]; then
        kill "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file"
        echo "FAIL: no foreground PID recorded"
        return 1
    fi

    kill -STOP "-$target_pid" 2>/dev/null || kill -STOP "$target_pid" 2>/dev/null

    if ! wait_for_process_exit "$cjsh_pid"; then
        kill -CONT "$cjsh_pid" 2>/dev/null || true
        kill -9 "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file"
        echo "FAIL: cjsh did not exit after SIGSTOP"
        return 1
    fi

    wait "$cjsh_pid"
    local exit_code=$?
    local output
    output="$(cat "$output_file" 2>/dev/null)"

    rm -f "$pid_file" "$output_file"

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code: $output"
        return 1
    fi

    if echo "$output" | grep -q "Running"; then
        echo "FAIL: job reported running after SIGSTOP: $output"
        return 1
    fi

    if echo "$output" | grep -q "Stopped"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected Stopped job output, got: $output"
    return 1
}

auto_background_pipeline() {
    log "Test: &^ auto-backgrounds pipeline"
    local pid_file output_file
    pid_file="$(mktemp /tmp/cjsh_autobg_pid.XXXXXX)"
    output_file="$(mktemp /tmp/cjsh_autobg_out.XXXXXX)"

    "$CJSH_PATH" -i -c "sh -c 'echo \$\$ > $pid_file; sleep 5' | cat &^; jobs; kill -9 %1 2>/dev/null; wait %1 2>/dev/null || true" >"$output_file" 2>&1 &
    local cjsh_pid=$!

    local target_pid
    target_pid="$(wait_for_pid_file "$pid_file")"
    if [ -z "$target_pid" ]; then
        kill "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file"
        echo "FAIL: no foreground PID recorded"
        return 1
    fi

    kill -TSTP "-$target_pid" 2>/dev/null || kill -TSTP "$target_pid" 2>/dev/null

    if ! wait_for_process_exit "$cjsh_pid"; then
        kill -CONT "$cjsh_pid" 2>/dev/null || true
        kill -9 "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file"
        echo "FAIL: cjsh did not exit after SIGTSTP"
        return 1
    fi

    wait "$cjsh_pid"
    local exit_code=$?
    local output
    output="$(cat "$output_file" 2>/dev/null)"

    rm -f "$pid_file" "$output_file"

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code: $output"
        return 1
    fi

    if echo "$output" | grep -q "Stopped"; then
        echo "FAIL: job reported stopped instead of running: $output"
        return 1
    fi

    if echo "$output" | grep -q "Running"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected Running job output, got: $output"
    return 1
}

auto_background_with_redirection() {
    log "Test: &^ works with redirection"
    local pid_file output_file redir_file
    pid_file="$(mktemp /tmp/cjsh_autobg_pid.XXXXXX)"
    output_file="$(mktemp /tmp/cjsh_autobg_out.XXXXXX)"
    redir_file="$(mktemp /tmp/cjsh_autobg_redir.XXXXXX)"

    "$CJSH_PATH" -i -c "sh -c 'echo \$\$ > $pid_file; sleep 5' > $redir_file &^; jobs; kill -9 %1 2>/dev/null; wait %1 2>/dev/null || true" >"$output_file" 2>&1 &
    local cjsh_pid=$!

    local target_pid
    target_pid="$(wait_for_pid_file "$pid_file")"
    if [ -z "$target_pid" ]; then
        kill "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file" "$redir_file"
        echo "FAIL: no foreground PID recorded"
        return 1
    fi

    kill -TSTP "$target_pid" 2>/dev/null

    if ! wait_for_process_exit "$cjsh_pid"; then
        kill -CONT "$cjsh_pid" 2>/dev/null || true
        kill -9 "$cjsh_pid" 2>/dev/null || true
        wait "$cjsh_pid" 2>/dev/null || true
        rm -f "$pid_file" "$output_file" "$redir_file"
        echo "FAIL: cjsh did not exit after SIGTSTP"
        return 1
    fi

    wait "$cjsh_pid"
    local exit_code=$?
    local output
    output="$(cat "$output_file" 2>/dev/null)"

    rm -f "$pid_file" "$output_file" "$redir_file"

    if [ $exit_code -ne 0 ]; then
        echo "FAIL: cjsh exited with $exit_code: $output"
        return 1
    fi

    if echo "$output" | grep -q "Stopped"; then
        echo "FAIL: job reported stopped instead of running: $output"
        return 1
    fi

    if echo "$output" | grep -q "Running"; then
        echo "PASS"
        return 0
    fi

    echo "FAIL: expected Running job output, got: $output"
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

if ! jobname_updates_jobs_output; then
    status=1
fi

if ! jobname_affects_command_matching; then
    status=1
fi

if ! jobname_rejects_empty_name; then
    status=1
fi

if ! auto_background_on_stop; then
    status=1
fi

if ! auto_background_no_caret_command; then
    status=1
fi

if ! auto_background_literal_amp_caret; then
    status=1
fi

if ! auto_background_ignores_sigstop; then
    status=1
fi

if ! auto_background_pipeline; then
    status=1
fi

if ! auto_background_with_redirection; then
    status=1
fi

exit $status
