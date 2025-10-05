# Contributing to CJ's Shell

## Code Standards

### Language and Style

- **C++**: Modern C++17/20 standards
- **Formatting**: All C/C++ code must conform to the `.clang-format` configuration (Google-based style with 4-space indentation)
- **Line Length**: 100 characters maximum
- **Indentation**: 4 spaces, no tabs
- **Braces**: Attach style (opening brace on same line)
- **Naming Conventions**:
  - Variables: `snake_case`
  - Functions: `snake_case`
  - Classes: `PascalCase`
  - Constants: `UPPER_SNAKE_CASE`
  - Global variables: `g_` prefix
  - Namespaces: `lowercase`

### Code Organization

- **Headers**: Place in `include/` directory with appropriate subdirectories
- **Source**: Place in `src/` directory mirroring `include/` structure
- **One class per file** when practical
- **Include guards**: Use `#pragma once`
- **Includes**: Group system headers, then third-party, then project headers
- **Smart pointers**: Prefer `std::unique_ptr` and `std::shared_ptr` over raw pointers
- **Error handling**: Use result types or explicit error checking; avoid exceptions in critical paths

### Documentation

- **Header files**: Document all public APIs with clear comments
- **Functions**: Describe purpose, parameters, return values, and side effects
- **Complex logic**: Add inline comments explaining non-obvious code
- **README updates**: Update documentation for new features

## Build System

### Building

```bash
# Clean build (recommended before testing)
./toolchain/build.sh --clean

# Normal build
./toolchain/build.sh
```

The build system uses the `nob` build tool located in `toolchain/nob/`.

### Build Requirements

- C++17-compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Standard POSIX environment

## Validation Procedure

### 1. Format Check

Before committing, ensure code is formatted:

```bash
# Format all C/C++ files
clang-format -i src/**/*.cpp include/**/*.h toolchain/nob/*.c toolchain/nob/*.h
```

### 2. Build Verification

```bash
# Clean build must succeed
./toolchain/build.sh --clean
```

Build must complete without errors or warnings.

### 3. Test Suite

**MANDATORY**: All tests must pass before submitting a pull request.

```bash
# Run full test suite (1000+ POSIX compliance tests)
./tests/run_shell_tests.sh
```

**Requirements**:
- Freshly compiled `cjsh` binary in `build/` directory
- No other `cjsh` instances running on the system
- Test suite cannot be run from within `cjsh`

**Test failures are blockers** - do not submit PRs with failing tests.

### 4. Test Writing Standards

When adding new features, write corresponding tests following the existing format:

- Test files: `tests/shell/test_<feature>.sh`
- Use strict test format as seen in existing test files
- Each test must output clear `PASS`, `FAIL`, or `SKIP` indicators
- Include both positive and negative test cases
- Test edge cases and error conditions

Example test structure:
```bash
#!/usr/bin/env sh
# Test description
CJSH=$1
test_count=0
pass_count=0

# Test case
test_count=$((test_count + 1))
if [ "expected" = "$($CJSH -c 'command')" ]; then
    echo "PASS: Test description"
    pass_count=$((pass_count + 1))
else
    echo "FAIL: Test description"
fi

# Exit with appropriate code
[ $pass_count -eq $test_count ]
```

## Pull Request Process

1. **Fork and branch**: Create a feature branch from `master`
2. **Code**: Implement your changes following all standards above
3. **Format**: Run `clang-format` on all modified files
4. **Build**: Verify clean build with `./toolchain/build.sh --clean`
5. **Test**: Run full test suite and ensure all tests pass
6. **Commit**: Write clear, descriptive commit messages
7. **PR**: Submit pull request with:
   - Clear description of changes
   - Reference to any related issues
   - Confirmation that tests pass
   - List of new test coverage if applicable

## Getting Help

- **Documentation**: See `docs/getting-started/`
- **Issues**: Check existing issues or open a new one
- **Questions**: Open a discussion issue before starting major changes

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
