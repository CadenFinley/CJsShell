# Development

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

To build a clean build of cjsh run;
```bash
    ./toolchain/build.sh --clean
```
