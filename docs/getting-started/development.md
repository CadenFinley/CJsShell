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

> **Heads up:** The harness launches every test script with Bash. Ensure `bash` is installed (or export `TEST_DRIVER_SHELL=/path/to/shell`) before running it locally or in CI runners that default to `dash`.

Please be sure to have a freshly compiled cjsh in the build directory before running. Also the test suite cannot be run inside or with another cjsh instance running on the same machine.

To produce a clean build of cjsh run:
```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel --clean-first
```

## Themes

I love prompt experiments, the more the merrier! If you have a BBCode-marked prompt, `PS1`/`RPS1` template, or `cjshopt style_def` palette you would like to share, please do so by opening a PR that adds it to the documentation or sample configs. There is no external theme DSL anymore, so everything lives in plain shell script—see the [Prompt Markup and Styling](../themes/thedetails.md) guide for the knobs you can use.

## Documentation 

Please. Anything helps.
