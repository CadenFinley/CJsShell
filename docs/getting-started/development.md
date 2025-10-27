# Want to help?

cjsh is in active development and pull requests and issue posts are always appreciated!

To get started follow the steps in quick start to clone the master branch! Thanks!

Please refer to CONTRIBUTING.md in the root of the repo for more in depth details about code format, style, and verification.

## Testing

cjsh has a very strict test suite, when going through it you will be able to see that each test script complies to a strict standard. When writing new tests for features you are implementing, please follow this format. 

The main test harness can be run like this:
```bash
    ./tests/run_shell_tests.sh
```

Please be sure to have a freshly compiled cjsh in the build directory before running. Also the test suite cannot be run inside or with another cjsh instance running on the same machine.

To produce a clean build of cjsh run:
```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel --clean-first
```

## Themes

I love themes, the more the merrier! If you have one you would like to contribute please do so! Themes can be hosted on the main repository if you would like. See the themes page for details on how to develop these and what tools you have at your disposal.

## Documentation 

Please. Anything helps.
