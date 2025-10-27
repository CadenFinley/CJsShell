# Contributing to CJsShell

## Testing and GitHub Workflows
- Every pull request and push to `master` triggers the **CI Build and Test** workflow (`.github/workflows/ci.yml`).
- CI builds the project with CMake in Release mode on both `ubuntu-latest` and `macos-latest`, using the same commands documented in the `ci.yml` workflow.
- The workflow runs `tests/run_shell_tests.sh`. Local contributors should run this script before opening a pull request to catch regressions early.
- macOS GitHub runners occasionally show flaky terminal-related tests. The workflow tolerates this only when the pass rate stays at or above 95%; treat any local failures as bugs unless you can reproduce the known runner quirk.

## Code Style and clang-format
- Format C and C++ sources with `clang-format` before submitting patches.
- The repository ships a project-specific configuration at `.clang-format` (Google base style, 4 space indentation, 100 character column limit, C++17 standard).
- Use `clang-format` 15 or newer to ensure full compatibility with the options in the configuration file.

## Language Standards
- C sources must compile as ISO C11 (`CMAKE_C_STANDARD 11`).
- C++ sources must compile as ISO C++17 (`CMAKE_CXX_STANDARD 17`).
- Do not introduce extensions that break these guarantees without discussing the change in an issue first.
