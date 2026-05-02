<!--
  development.md

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
-->

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

I love prompt experiments, the more the merrier! If you have a BBCode-marked prompt, `PS1`/`RPS1` template, or `cjshopt style_def` palette you would like to share, please do so by opening a PR that adds it to the documentation or sample configs. There is no external theme DSL anymore, so everything lives in plain shell script—see the [Prompt Markup and Styling](../themes/thedetails.md) guide for the knobs you can use.

## Documentation 

Please. Anything helps.
