# summarize_ctest.awk
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

# Count shell PASS/FAIL lines and aggregate focused suites' reported summaries.
# Parse only test output, never echoed command arguments.

function finish_suite() {
    if (name == "") return
    suites++
    if (!known && name ~ /^(agent_mode_interactive_tests|idle_hook_interactive_tests|job_notification_interactive_tests|generate_completions_integration_tests)$/) {
        # These scripts each exercise a single continuous integration scenario.
        count = 1
        failures = (status != "passed")
        known = 1
    }
    if (!known || count == 0 || failures + skipped > count) {
        unavailable++
        return
    }
    total += count
    failed += failures
    skipped_total += skipped
    passed += count - failures - skipped
    # A crash or timeout after a passing summary is still a failed suite, and
    # its remaining individual results cannot be inferred from that summary.
    if (status != "passed" && failures == 0) unavailable++
}

function summary_value(line, key,    value) {
    if (match(line, "[(,][[:space:]]*" key "=[0-9]+")) {
        value = substr(line, RSTART, RLENGTH)
        sub(/^.*=/, "", value)
        return value + 0
    }
    return 0
}

BEGIN {
    ansi = sprintf("%c", 27) "\\[[0-9;]*[A-Za-z]"
}

!in_output && /^[0-9]+\/[0-9]+ Testing: / {
    finish_suite()
    name = $0
    sub(/^[0-9]+\/[0-9]+ Testing: /, "", name)
    count = failures = skipped = known = in_output = 0
    status = ""
    next
}

/^Output:$/ { in_output = 1; next }
/^<end of output>$/ { in_output = 0; next }

!in_output {
    if ($0 ~ /^Test Passed\./ || $0 ~ /^Test Pass Reason:/) status = "passed"
    if ($0 ~ /^Test Failed\./ || $0 ~ /^Test Fail Reason:/) status = "failed"
    next
}

{
    gsub(ansi, "")
    if (name ~ /^shell\./) {
        # Shell files report individual results in several formats, including
        # "PASS: description", "[PASS] description", and "description: PASS".
        if ($0 ~ /(^|[^A-Za-z0-9_])PASS([^A-Za-z0-9_]|$)/) {
            count++
            known = 1
        }
        if ($0 ~ /(^|[^A-Za-z0-9_])FAIL([^A-Za-z0-9_]|$)/) {
            count++
            failures++
            known = 1
        }
        next
    }
    if ($0 ~ /^All [0-9]+ .*tests passed/) {
        count = $2 + 0
        failures = 0
        known = 1
    } else if ($0 ~ /^[0-9]+\/[0-9]+ .*tests failed/) {
        split($1, fields, "/")
        count = fields[2] + 0
        failures = fields[1] + 0
        known = 1
    } else if ($0 ~ /^Loop syntax: [0-9]+ checks, [0-9]+ failures$/) {
        count = $3 + 0
        failures = $5 + 0
        known = 1
    } else if ($0 ~ /^PASS: all [0-9]+ child launches preserved SIGTERM status$/) {
        count = $3 + 0
        failures = 0
        known = 1
    } else if ($0 ~ /^FAIL: [0-9]+\/[0-9]+ child launches lost SIGTERM$/) {
        split($2, fields, "/")
        count = fields[2] + 0
        failures = fields[1] + 0
        known = 1
    } else if ($0 ~ /^Ran [0-9]+ tests? in /) {
        count = $2 + 0
        known = 1
    } else if ($0 ~ /^(OK|FAILED)( \(|$)/) {
        skipped = summary_value($0, "skipped") + summary_value($0, "expected failures")
        failures = summary_value($0, "failures") + summary_value($0, "errors") + summary_value($0, "unexpected successes")
    }
}

END {
    finish_suite()
    if (suites == 0) exit
    printf "\nTotal individual tests: %d\n", total
    printf "Passed: %d\nFailed: %d\nSkipped: %d\n", passed, failed, skipped_total
    if (unavailable) {
        printf "Suites without complete individual results: %d\n", unavailable
    } else if (total > skipped_total) {
        printf "%g%% individual tests passed out of %d executed\n", 100 * passed / (total - skipped_total), total - skipped_total
    }
}
