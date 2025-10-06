# Quick Start

## Prerequisites

A standard C/C++ Compiler:

 - GCC
 - clang

And that is it! cjsh has no external dependencies and was designed like this for pure simplicity. To just work where ever.

## Installation

cjsh can be downloaded or built in multiple different ways.

### As a package

cjsh is packaged through brew and it is as simple as this:

```bash
    brew tap CadenFinley/tap
    brew install cjsh
```
This also works on linux through linuxbrew.

Hopefully more package managers to come as cjsh gets bigger.

### Manual building and installation

cjsh is super easy to download and install. Everything is hosted on the github repo at: `https://github.com/CadenFinley/CJsShell` 

The master branch holds the most recent commits and may not always be stable and may have breaking changes with no backwards compatibility. For the most stable release, stick to using the latest tagged release from the public GitHub releases.

cjsh is still in active, rapid development so even the latest release can still have breaking changes with no backwards compatibilities, although they will always be noted in the release if they exist.

```bash
    # First clone the repo
    git clone https://github.com/CadenFinley/CJsShell && cd CJsShell

    # Run the build script
    chmod +x toolchain/build.sh && ./toolchain/build.sh

    # Use the debug installer to install to your path
    chmod +x toolchain/debug_install.sh && ./toolchain/debug_install.sh
```

The debug installer installs automatically to the most sensible path for the user

## Build info

Build configuration is automatically handled by nob and requires no extra steps. There are multiple build types and can be seen with the --help flag. Build status and origin is tracked within nob and is embedded within the cjsh executable and is displayed in its version. cjsh takes full advantage of many optimizer compilation flags so compilation and linking can take some time.





    
