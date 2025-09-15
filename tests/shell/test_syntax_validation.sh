#!/usr/bin/env sh
# Test comprehensive syntax validation edge cases
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: syntax validation edge cases..."

# Create temporary test directory
TEST_DIR="/tmp/cjsh_syntax_edge_tests_$$"
mkdir -p "$TEST_DIR"

# Test 1: Nested control structures (good)
cat > "$TEST_DIR/nested_good.sh" << 'EOF'
#!/bin/bash
if [ "$1" = "test" ]; then
    for i in 1 2 3; do
        while [ $i -lt 10 ]; do
            echo $i
            i=$((i + 1))
        done
    done
fi
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/nested_good.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: nested control structures (good) should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 2: Nested control structures (bad - missing inner done)
cat > "$TEST_DIR/nested_bad.sh" << 'EOF'
#!/bin/bash
if [ "$1" = "test" ]; then
    for i in 1 2 3; do
        while [ $i -lt 10 ]; do
            echo $i
            i=$((i + 1))
        # Missing done for while
    done
fi
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/nested_bad.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "ERROR"; then
    echo "FAIL: nested control structures (bad) should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 3: Complex quoting scenarios (good)
cat > "$TEST_DIR/quoting_good.sh" << 'EOF'
#!/bin/bash
echo "Hello \"world\""
echo 'Single quotes with "double quotes" inside'
echo "Double quotes with 'single quotes' inside"
echo "Escaped \$variable"
echo 'Literal $variable'
echo "Command substitution: $(date)"
echo "Arithmetic: $((2 + 2))"
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/quoting_good.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: complex quoting (good) should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 4: Unbalanced quotes in different contexts
cat > "$TEST_DIR/quoting_bad.sh" << 'EOF'
#!/bin/bash
echo "Unbalanced double quote
echo 'Unbalanced single quote
echo "Mixed quote types'
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/quoting_bad.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "ERROR"; then
    echo "FAIL: unbalanced quotes should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 5: Function definitions (good)
cat > "$TEST_DIR/functions_good.sh" << 'EOF'
#!/bin/bash
function func1() {
    echo "function 1"
}

func2() {
    echo "function 2"
    return 0
}

# Function with local variables
func3() {
    local var1="test"
    local var2=42
    echo "$var1 $var2"
}

# Recursive function
factorial() {
    if [ $1 -le 1 ]; then
        echo 1
    else
        local result=$(factorial $(($1 - 1)))
        echo $(($1 * result))
    fi
}
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/functions_good.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: function definitions (good) should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 6: Function definitions (bad)
cat > "$TEST_DIR/functions_bad.sh" << 'EOF'
#!/bin/bash
function bad_func1() {
    echo "missing closing brace"

function bad_func2() {
    echo "another function"
    # Missing }

# Malformed function declaration
function bad_func3( {
    echo "syntax error"
}
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/functions_bad.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "ERROR"; then
    echo "FAIL: function definitions (bad) should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 7: Command substitution and arithmetic (good)
cat > "$TEST_DIR/substitution_good.sh" << 'EOF'
#!/bin/bash
DATE=$(date)
COUNT=$((1 + 2 + 3))
NESTED=$(echo $(date))
ARITHMETIC=$(($(echo 5) * 2))
BACKTICKS=`date`
echo "$DATE $COUNT $NESTED $ARITHMETIC $BACKTICKS"
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/substitution_good.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: command substitution (good) should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 8: Command substitution and arithmetic (bad)
cat > "$TEST_DIR/substitution_bad.sh" << 'EOF'
#!/bin/bash
BAD_CMD=$(date
BAD_ARITH=$((1 + 2
BAD_NESTED=$(echo $(date)
UNCLOSED_BACKTICKS=`date
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/substitution_bad.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "ERROR"; then
    echo "FAIL: command substitution (bad) should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 9: Simple conditional statements (good)
cat > "$TEST_DIR/conditional_good.sh" << 'EOF'
#!/bin/bash
if [ "$1" = "start" ]; then
    echo "Starting service"
elif [ "$1" = "stop" ]; then
    echo "Stopping service"
elif [ "$1" = "restart" ]; then
    echo "Restarting service"
else
    echo "Unknown option: $1"
    exit 1
fi
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/conditional_good.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: conditional statements (good) should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 10: Conditional statements (bad)
cat > "$TEST_DIR/conditional_bad.sh" << 'EOF'
#!/bin/bash
if [ "$1" = "start" ]; then
    echo "Starting"
    # Missing fi
elif [ "$1" = "stop" ]; then
    echo "Stopping"
# Missing fi for the whole if block

# Another bad conditional
if [ true; then
    echo "malformed if"
fi
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/conditional_bad.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] || ! echo "$OUT" | grep -q "ERROR"; then
    echo "FAIL: conditional statements (bad) should fail (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 11: Comments and special characters
cat > "$TEST_DIR/comments_good.sh" << 'EOF'
#!/bin/bash
# This is a comment
echo "Hello" # Inline comment
# Comment with special chars: @#$%^&*()
: << 'EOF_COMMENT'
This is a here document comment
Multiple lines
Special chars: !@#$%^&*()
EOF_COMMENT

echo "After comment block"
EOF

OUT=$("$CJSH_PATH" -c "syntax $TEST_DIR/comments_good.sh" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: comments and special characters should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Test 12: Test with -c option and simple script
OUT=$("$CJSH_PATH" -c "syntax -c 'echo hello world'" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] || ! echo "$OUT" | grep -q "No syntax errors found"; then
    echo "FAIL: syntax -c with simple command should pass (exit: $EXIT_CODE, output: '$OUT')"
    rm -rf "$TEST_DIR"
    exit 1
fi

# Clean up
rm -rf "$TEST_DIR"

echo "PASS: nested control structures (good)"
echo "PASS: nested control structures (bad)"
echo "PASS: complex quoting scenarios (good)"
echo "PASS: unbalanced quotes in different contexts"
echo "PASS: function definitions (good)"
echo "PASS: function definitions (bad)"
echo "PASS: command substitution and arithmetic (good)"
echo "PASS: command substitution and arithmetic (bad)"
echo "PASS: simple conditional statements (good)"
echo "PASS: conditional statements (bad)"
echo "PASS: comments and special characters"
echo "PASS: syntax -c option with simple script"
exit 0
