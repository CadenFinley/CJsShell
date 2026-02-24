# Contributing to CJsShell

## Testing and GitHub Workflows
- Every pull request and push to `master` triggers the **CI Build and Test** workflow (`.github/workflows/ci.yml`).
- CI builds the project with CMake in Release mode on both `ubuntu-latest` and `macos-latest`, using the same commands documented in the `ci.yml` workflow.
- The workflow runs `tests/run_shell_tests.sh`. Local contributors should run this script before opening a pull request to catch regressions early.

## Code Style and clang-format
- Format C and C++ sources with `clang-format` before submitting patches.
- The repository ships a project-specific configuration at `.clang-format` (Google base style, 4 space indentation, 100 character column limit, C++17 standard).
- Use `clang-format` 15 or newer to ensure full compatibility with the options in the configuration file.

## Language Standards
- C sources must compile as ISO C11 (`CMAKE_C_STANDARD 11`).
- C++ sources must compile as ISO C++17 (`CMAKE_CXX_STANDARD 17`).
- Do not introduce extensions that break these guarantees without discussing the change in an issue first.

## Known Issues
- automattic line reflowing is currently disabled in isocline via signal_handler.cpp, there are some serious issues regarding the way the lines are redrawn. please see signal_handler.cpp for more details. this is a fairly involved issue in isocline and will require more effort than normal to fix, so this would be a great issue for someone to start with if they would really like to contribute.
- on rare occasions, cjsh will not exit for shut itself down properly and will enter an infinte input loop as a zombie process. this is a known issue that is being looked into. what is known is that this loop is not strictly within isocline. I beleive that isocline is returning and the loop is actually within the main cjsh loop

## Upcoming Features/ Want to implement Features
- At the moment history is handled entirely transiently. Reads and writes are done at instance and immeadiatly close the file after. What this allows is history to be transient between all cjsh instances. This is good, but I know people would like to have a way to disable this so that history is read once on shell startup and saved in memory and then written back to the file on shell exit. This would be a pretty involved change, but nonetheless a great one to implement.

## Good First issues
- A lot of cjsh needs some heavy refactoring and cleanup. This is a great oportunity to learn the codebase and make some signifigant contributions. Namely the parser and interpreter modules are the main areas on intrest. These are greate areas to test as they are heavily tested and changes would be easy to verify as they would likely cause test failures if not done correctly.
- A lot of test fail paths still say 'not implemented yet' instead of a proper error message. This would be a greate area to contribute to and would not take much effort.
- A lot of error messages can be a little wordy and could be simplified. Additionaly, suggestions are not provided for all error messages and some could actually use some. This is a great area to contribute to as well and would not take much effort.
- A great way to contribute is actually just to use the shell for day-to-day use and report any and all bugs you find. This could also be a great path into finding some good first issues to solve.
- Migrate disown, and kill builtin files into the main job control commands builtins files
