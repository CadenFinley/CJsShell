# CJ's Shell Test Suite

This directory contains a comprehensive test suite for CJ's Shell (cjsh) designed to ensure reliability, POSIX compliance, and feature correctness. The test suite includes over 200 individual tests covering core shell functionality, POSIX compliance, error handling, and performance.

## Quick Start

To run the complete test suite:

```bash
./tests/run_shell_tests.sh
```

To run individual test categories or files:

```bash
# Run a specific test file
sh tests/shell/test_posix_compliance.sh

# Run quick POSIX check
./tests/quick_posix_check.sh

# Compare with other shells
./tests/compare_shells.sh
```

## Test Structure

### Main Test Scripts

- **`run_shell_tests.sh`** - Master test runner that executes all test categories
- **`quick_posix_check.sh`** - Fast POSIX compliance check for development
- **`compare_shells.sh`** - Compare cjsh behavior against other shells (sh, bash, zsh)
- **`debug_posix_issues.sh`** - Debug script for POSIX compliance issues

### Test Categories

The test suite is organized into the following categories:

#### Core Tests
- **`test_builtin_commands.sh`** - Built-in commands (echo, printf, version, help, test)
- **`test_environment_vars.sh`** - Environment variable handling and expansion
- **`test_command_line_options.sh`** - Command-line flags and options

#### File I/O Tests
- **`test_file_operations.sh`** - File system operations and path handling
- **`test_redirections.sh`** - Input/output redirection (>, <, >>, 2>, &>, etc.)
- **`test_posix_io.sh`** - POSIX I/O redirection compliance

#### Scripting Tests
- **`test_scripting.sh`** - Shell script execution and interpretation
- **`test_control_structures.sh`** - Control flow (if/then/else, loops, case)
- **`test_quoting_expansions.sh`** - Quote handling and parameter expansion
- **`test_posix_variables.sh`** - POSIX variable expansion and substitution

#### Process Management Tests
- **`test_process_management.sh`** - Process creation and management
- **`test_pipeline.sh`** - Pipeline execution and chaining
- **`test_job_control.sh`** - Background jobs and job control
- **`test_and_or.sh`** - Logical operators (&& and ||)
- **`test_posix_signals.sh`** - Signal handling compliance

#### Shell Feature Tests
- **`test_alias.sh`** - Command aliases
- **`test_cd.sh`** - Directory navigation
- **`test_cd_edges.sh`** - Edge cases for cd command
- **`test_export.sh`** - Environment variable export

#### Advanced Feature Tests
- **`test_login_shell.sh`** - Login shell behavior and initialization
- **`test_posix_login_env.sh`** - POSIX login environment compliance

#### POSIX Compliance Tests
- **`test_posix_compliance.sh`** - Core POSIX shell standard compliance
- **`test_posix_advanced.sh`** - Advanced POSIX features
- **`test_posix_builtins.sh`** - POSIX built-in command compliance
- **`test_globbing.sh`** - Filename globbing and pattern matching

#### Edge Case and Error Tests
- **`test_error_handling.sh`** - Error handling and recovery
- **`test_error_edge_cases.sh`** - Edge cases and boundary conditions
- **`test_misc_edges.sh`** - Miscellaneous edge cases

#### Performance Tests
- **`test_performance.sh`** - Performance benchmarks and stress tests

## POSIX Compliance Status

CJ's Shell maintains approximately **75% POSIX compliance** with strong support in core areas:

### ✅ Fully Supported POSIX Features
- Basic command execution and pipelines
- Environment variable handling
- I/O redirection (basic forms)
- Login shell initialization
- Core built-in commands (cd, export, echo, etc.)
- Process management and job control (basic)
- Filename globbing and pattern matching

### ⚠️ Partially Supported POSIX Features
- Advanced parameter expansion (`${var:offset:length}`, etc.)
- Complex built-in commands (complete `test` command syntax)
- Advanced I/O redirection forms
- Signal handling edge cases
- Some advanced variable substitution patterns

### ❌ Known Limitations
- Complete `set` built-in compliance
- Some advanced shell function features
- Certain signal handling edge cases
- Full compliance with all POSIX parameter expansion forms

## Running Tests

### Prerequisites

1. **Build cjsh**: Tests expect the binary at `build/cjsh`
   ```bash
   mkdir build && cd build
   cmake ..
   make -j
   ```

2. **Required tools**: Standard POSIX utilities (sh, grep, sed, awk)

### Test Execution

#### Complete Test Suite
```bash
# Run all tests with detailed output
./tests/run_shell_tests.sh

# Expected output includes:
# - Test results by category
# - Pass/fail counts
# - POSIX compliance summary
# - Performance metrics
```

#### Individual Test Categories
```bash
# Run only core functionality tests
sh tests/shell/test_builtin_commands.sh
sh tests/shell/test_environment_vars.sh

# Run POSIX compliance tests
sh tests/shell/test_posix_compliance.sh
sh tests/shell/test_posix_advanced.sh
```

#### Development Testing
```bash
# Quick POSIX check during development
./tests/quick_posix_check.sh

# Debug specific POSIX issues
./tests/debug_posix_issues.sh
```

#### Comparative Testing
```bash
# Compare behavior with other shells
./tests/compare_shells.sh

# Test against specific shell
sh tests/shell/test_posix_compliance.sh /bin/bash
```

## Test Output Format

### Standard Test Output
```
Running CJ's Shell Test Suite
=============================
Testing binary: /path/to/build/cjsh

=== Core Tests ===
  test_builtin_commands:         PASS
  test_environment_vars:         PASS
  test_command_line_options:     PASS

=== POSIX Compliance Tests ===
  test_posix_compliance:         PASS
  test_posix_advanced:           FAIL
    Output: Advanced parameter expansion not fully supported

Test Summary
============
Total tests: 25
Passed: 23
Failed: 2
Warnings: 1

POSIX Compliance Summary
========================
✅ Core POSIX Features
⚠️ Advanced POSIX Features (partial)
✅ Login Shell Environment

Estimated POSIX Compliance: ~75%
```

### Individual Test Format
Each test file outputs results in this format:
```bash
Test 001: Basic command execution... PASS
Test 002: Pipeline execution... PASS
Test 003: Parameter expansion... FAIL - Advanced syntax not supported
```

## Writing New Tests

### Test File Template
```bash
#!/usr/bin/env sh
# Description of what this test file covers

# Use environment variable or default path
if [ -n "$CJSH" ]; then 
    CJSH_PATH="$CJSH"
else 
    CJSH_PATH="$(cd "$(dirname "$0")/../../build" && pwd)/cjsh"
fi

echo "Test: [test description]..."

# Test case 1
OUT=$("$CJSH_PATH" -c "command to test")
if [ "$OUT" != "expected output" ]; then
    echo "FAIL: test case description (got '$OUT')"
    exit 1
fi

# Test case 2
"$CJSH_PATH" -c "command that should succeed"
if [ $? -ne 0 ]; then
    echo "FAIL: test case description"
    exit 1
fi

echo "PASS: All tests passed"
exit 0
```

### Test Guidelines

1. **Exit Codes**: Return 0 for success, non-zero for failure
2. **Error Messages**: Include descriptive failure messages with actual vs expected output
3. **Cleanup**: Clean up any temporary files or processes
4. **Isolation**: Tests should not depend on each other
5. **POSIX Compliance**: Use only POSIX-compliant shell constructs in test scripts

### Adding Tests to Categories

To add a new test to the main test runner:

1. Create the test file in `tests/shell/`
2. Add the test name (without `.sh`) to the appropriate category in `run_shell_tests.sh`
3. Test the individual file before adding to the suite

Example:
```bash
# In run_shell_tests.sh, add to appropriate category:
CORE_TESTS="test_builtin_commands test_environment_vars test_your_new_test"
```

## Continuous Integration

The test suite is designed to work with CI/CD systems:

### Return Codes
- `0`: All tests passed
- `> 0`: Number of failed tests (up to 255)

### Environment Variables
- `CJSH`: Path to cjsh binary (overrides default)
- `NO_COLOR`: Disable colored output for CI systems

### CI Usage Example
```bash
# In CI script
if ! ./tests/run_shell_tests.sh; then
    echo "Tests failed, see output above"
    exit 1
fi
```

## Debugging Failed Tests

### Common Issues
1. **Binary not found**: Ensure `build/cjsh` exists and is executable
2. **Permission errors**: Check file permissions for test scripts
3. **Environment differences**: Some tests may be sensitive to PATH or other env vars

### Debug Commands
```bash
# Run with verbose output
sh -x tests/shell/test_name.sh

# Test specific shell binary
CJSH=/path/to/cjsh ./tests/run_shell_tests.sh

# Compare with reference shell
./tests/compare_shells.sh
```

## Contributing to Tests

### Test Coverage Goals
- **Functionality**: Every built-in command and feature
- **POSIX Compliance**: Core POSIX shell standard requirements
- **Edge Cases**: Boundary conditions and error scenarios
- **Performance**: Ensure reasonable performance characteristics

### Before Submitting
1. Run the complete test suite
2. Ensure new tests follow the template format
3. Add tests to appropriate categories
4. Update this README if adding new test categories

## Test Maintenance

### Regular Updates
- Add tests for new features
- Update POSIX compliance tests as standards evolve
- Maintain compatibility with different POSIX-compliant systems
- Update performance benchmarks

### Platform Considerations
Tests are designed to work on:
- **macOS** (primary development platform)
- **Linux** (various distributions)
- **Windows** (via WSL or Cygwin)
- **FreeBSD** and other UNIX-like systems

---

For questions about the test suite or to report test failures, please open an issue on the [CJ's Shell GitHub repository](https://github.com/CadenFinley/CJsShell).
