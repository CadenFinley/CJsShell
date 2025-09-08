# CJ's Shell Test Suite

This directory contains a comprehensive test suite for CJ's Shell (cjsh) designed to ensure reliability, POSIX compliance, and feature correctness. The test suite includes **29 test categories** covering core shell functionality, POSIX compliance, error handling, and performance with **100% pass rate**.

## Quick Start

To run the complete test suite:

```bash
./tests/run_shell_tests.sh
```

To run individual test categories or files:

```bash
# Run a specific test file
sh tests/shell/test_posix_compliance.sh
```

## Test Structure

### Main Test Scripts

- **`run_shell_tests.sh`** - Master test runner that executes all test categories

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

CJ's Shell maintains approximately **90% POSIX compliance** with excellent support across all core areas:

### Fully Supported POSIX Features
- Basic command execution and pipelines (100% compliant)
- Environment variable handling and expansion (100% compliant)
- Core I/O redirection (>, <, >>, 2>, &>, etc.)
- Login shell initialization and environment setup
- All core built-in commands (cd, export, echo, test, etc.)
- Process management and job control
- Filename globbing and pattern matching
- POSIX variables and parameter expansion
- Control structures (if/then/else, loops, case)
- Command quoting and expansions
- Logical operators (&& and ||)
- Pipeline execution and chaining

### Partially Supported POSIX Features
- Advanced I/O redirection forms (noclobber, force overwrite >|, here-doc tab stripping <<-)
- Some interactive signal handling features
- Advanced shell function features (function local variables)
- System profile sourcing in controlled environments

### Known Limitations
- Custom file descriptor redirection
- Noclobber behavior (set -C)
- Interactive signal handling (SIGINT in non-interactive contexts)
- Some terminal control features requiring interactive sessions

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
```

## Test Maintenance

### Current Status (September 2025)
The test suite is in excellent condition with:
- **100% test pass rate** across all 29 test categories
- **~90% POSIX compliance** with strong core feature support
- **Comprehensive coverage** of shell functionality
- **Robust error handling** and edge case coverage
- **Performance benchmarks** consistently met

### Regular Updates
- Add tests for new features
- Update POSIX compliance tests as standards evolve
- Maintain compatibility with different POSIX-compliant systems
- Update performance benchmarks
- Monitor and address any new edge cases

### Platform Considerations
Tests are designed to work on:
- **macOS** (primary development platform)
- **Linux** (various distributions)
- **Windows** (via WSL or Cygwin)
- **FreeBSD** and other UNIX-like systems

---

For questions about the test suite or to report test failures, please open an issue on the [CJ's Shell GitHub repository](https://github.com/CadenFinley/CJsShell).
