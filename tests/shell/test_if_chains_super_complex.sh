#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: super complex nested if script..."

TESTS_PASSED=0
TESTS_FAILED=0

pass_test() {
    echo "PASS: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail_test() {
    echo "FAIL: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

create_complex_script() {
    SCRIPT_PATH=$1
    cat <<'EOF' > "$SCRIPT_PATH"
#!/usr/bin/env sh

report_stage() {
    stage=$1
    detail=$2

    if [ "$stage" = "alpha" ]; then
        if [ "$detail" = "ok" ]; then
            printf '%s:%s\n' "$stage" "$detail"
        elif [ "${detail#fallback-}" != "$detail" ]; then
            printf '%s:%s\n' "$stage" "$detail"
        else
            printf '%s:%s\n' "$stage" "$detail"
        fi
    else
        if [ "$stage" = "beta" ] || [ "$stage" = "gamma" ]; then
            printf '%s:%s\n' "$stage" "$detail"
        else
            printf '%s:%s\n' "$stage" "$detail"
        fi
    fi
}

evaluate_level() {
    label=$1
    value=$2

    if [ "$label" = "beta" ]; then
        if [ "$value" -gt 2 ]; then
            echo "beta-gt"
            return 0
        elif [ "$value" -eq 2 ]; then
            echo "beta-eq"
            return 1
        else
            echo "beta-lt"
            return 2
        fi
    elif [ "$label" = "gamma" ]; then
        if [ "$value" -gt 5 ]; then
            mod=$((value % 2))
            if [ "$mod" -eq 0 ]; then
                echo "gamma-even"
                return 0
            else
                echo "gamma-odd"
                return 1
            fi
        else
            if [ "$value" -gt 0 ]; then
                echo "gamma-small"
                return 3
            else
                echo "gamma-zero"
                return 4
            fi
        fi
    else
        echo "alpha-ok"
        return 0
    fi
}

: "${STAGE_ALPHA_READY:=1}"
: "${FALLBACK_STAGE:=rescue}"
: "${BETA_VALUE:=5}"
: "${GAMMA_VALUE:=9}"

summary_flag=0
warnings=0

for stage in alpha beta gamma; do
    case "$stage" in
        alpha)
            if [ "$STAGE_ALPHA_READY" -eq 1 ]; then
                detail="ok"
            else
                if [ -n "$FALLBACK_STAGE" ]; then
                    if [ "$FALLBACK_STAGE" = "rescue" ]; then
                        detail="fallback-rescue"
                        warnings=$((warnings + 1))
                    else
                        detail="fallback-generic"
                        warnings=$((warnings + 1))
                    fi
                else
                    detail="fail"
                    summary_flag=1
                fi
            fi
            ;;
        beta)
            if level_output=$(evaluate_level "$stage" "$BETA_VALUE"); then
                detail="$level_output"
            else
                status=$?
                if [ "$status" -eq 1 ]; then
                    detail="beta-warning:$level_output"
                elif [ "$status" -eq 2 ]; then
                    detail="beta-error:$level_output"
                    summary_flag=1
                else
                    detail="beta-unknown:$status"
                    summary_flag=1
                fi
            fi
            ;;
        gamma)
            if level_output=$(evaluate_level "$stage" "$GAMMA_VALUE"); then
                detail="$level_output"
            else
                status=$?
                if [ "$status" -eq 1 ]; then
                    if [ "$GAMMA_VALUE" -gt 7 ]; then
                        detail="gamma-odd-high"
                    else
                        detail="gamma-odd-low"
                    fi
                elif [ "$status" -eq 3 ]; then
                    detail="gamma-small"
                elif [ "$status" -eq 4 ]; then
                    detail="gamma-zero"
                    summary_flag=1
                else
                    detail="gamma-issue:$status"
                    summary_flag=1
                fi
            fi
            ;;
        *)
            detail="unknown"
            summary_flag=1
            ;;
    esac

    report_stage "$stage" "$detail"
done

score=0
for value in 1 2 3; do
    parity=$(((value + BETA_VALUE) % 2))
    if [ "$parity" -eq 0 ]; then
        score=$((score + 2))
    else
        if [ "$value" -gt 2 ]; then
            score=$((score + 2))
        else
            score=$((score + 1))
        fi
    fi
done

if [ "$score" -ge 5 ]; then
    echo "score:balanced"
else
    echo "score:low"
    summary_flag=1
fi

echo "warnings:$warnings"

if [ "$summary_flag" -eq 0 ]; then
    echo "summary:pass"
else
    echo "summary:fail"
fi
EOF
    chmod +x "$SCRIPT_PATH"
}

TEMP_SCRIPT="/tmp/cjsh_complex_if_script_$$.sh"
create_complex_script "$TEMP_SCRIPT"

OUTPUT=$("$CJSH_PATH" "$TEMP_SCRIPT")
EXPECTED="alpha:ok
beta:beta-gt
gamma:gamma-odd-high
score:balanced
warnings:0
summary:pass"

if [ "$OUTPUT" = "$EXPECTED" ]; then
    pass_test "complex script default path"
else
    fail_test "complex script default path (got: '$OUTPUT')"
fi

OUTPUT=$(STAGE_ALPHA_READY=0 BETA_VALUE=1 GAMMA_VALUE=2 FALLBACK_STAGE= "$CJSH_PATH" "$TEMP_SCRIPT")
EXPECTED="alpha:fail
beta:beta-error:beta-lt
gamma:gamma-small
score:balanced
warnings:0
summary:fail"

if [ "$OUTPUT" = "$EXPECTED" ]; then
    pass_test "complex script failure path"
else
    fail_test "complex script failure path (got: '$OUTPUT')"
fi

rm -f "$TEMP_SCRIPT"

echo
echo "PASSED: $TESTS_PASSED"
echo "FAILED: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
