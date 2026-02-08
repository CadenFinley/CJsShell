#!/usr/bin/env sh
if [ -n "$CJSH" ]; then CJSH_PATH="$CJSH"; else CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"; fi
echo "Test: PATH variable assignment and inheritance..."

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

if "$CJSH_PATH" -c 'PATH="/custom/bin:$PATH"; echo $PATH' | grep -q "/custom/bin"; then
    pass_test "PATH assignment updates shell variable"
else
    fail_test "PATH assignment updates shell variable"
fi

if "$CJSH_PATH" -c 'PATH="/custom/bin:$PATH"; '"$CJSH_PATH"' -c "echo \$PATH"' | grep -q "/custom/bin"; then
    pass_test "PATH assignment inherited by child processes"
else
    fail_test "PATH assignment inherited by child processes"
fi

if "$CJSH_PATH" -c 'PATH="/test1:$PATH"; export PATH="/test2:$PATH"; echo $PATH' | grep -q "/test2:/test1"; then
    pass_test "PATH assignment works with subsequent export using \$PATH"
else
    fail_test "PATH assignment works with subsequent export using \$PATH"
fi

BASH_RESULT=$(bash -c 'PATH="/custom/bin:$PATH"; bash -c "echo \$PATH"' | grep -c "/custom/bin")
CJSH_RESULT=$("$CJSH_PATH" -c 'PATH="/custom/bin:$PATH"; '"$CJSH_PATH"' -c "echo \$PATH"' | grep -c "/custom/bin")

if [ "$BASH_RESULT" = "$CJSH_RESULT" ]; then
    pass_test "CJsh PATH behavior matches bash"
else
    fail_test "CJsh PATH behavior matches bash (bash: $BASH_RESULT, cjsh: $CJSH_RESULT)"
fi

if "$CJSH_PATH" -c 'MYVAR="test"; '"$CJSH_PATH"' -c "echo \$MYVAR"' | grep -q "test"; then
    fail_test "Regular variables should not be automatically exported"
else
    pass_test "Regular variables not automatically exported"
fi

TEMP_CARGO_DIR="/tmp/test_cargo_bin_$$"
mkdir -p "$TEMP_CARGO_DIR"
echo "#!/bin/sh" > "$TEMP_CARGO_DIR/testbin"
echo "echo 'test command found'" >> "$TEMP_CARGO_DIR/testbin"
chmod +x "$TEMP_CARGO_DIR/testbin"

if "$CJSH_PATH" -c "PATH=\"\$PATH:$TEMP_CARGO_DIR\"; which testbin" | grep -q "$TEMP_CARGO_DIR/testbin"; then
    pass_test "PATH assignment allows finding new commands"
else
    fail_test "PATH assignment allows finding new commands"
fi

LLVM_PATH="/usr/local/fake-llvm/bin"
if "$CJSH_PATH" -c "PATH=\"\$PATH:$TEMP_CARGO_DIR\"; export PATH=\"$LLVM_PATH:\$PATH\"; echo \$PATH" | grep -q "$TEMP_CARGO_DIR"; then
    pass_test "PATH assignment survives subsequent export with command substitution"
else
    fail_test "PATH assignment survives subsequent export with command substitution"
fi

rm -rf "$TEMP_CARGO_DIR"

echo
echo "PATH Assignment Test Summary:"
echo "PASSED: $TESTS_PASSED"
echo "FAILED: $TESTS_FAILED"
if [ $TESTS_FAILED -eq 0 ]; then
    echo "All PATH assignment tests passed!"
    exit 0
else
    echo "Some PATH assignment tests failed!"
    exit 1
fi
