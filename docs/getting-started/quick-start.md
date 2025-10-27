# Quick Start

## Prerequisites

A standard C/C++ Compiler:

 - GCC
 - clang

And CMake 3.25 or newer.

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

    # Configure a Release build (outputs to ./build)
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

    # Compile using all available cores
    cmake --build build --config Release --parallel
```

After building, the `cjsh` executable will be in the `build/` directory. You can run it directly with `./build/cjsh`
## Build info

By default the commands above produce an optimized Release build.

- Pass `-DCMAKE_BUILD_TYPE=Debug` to build with sanitizers and full debug info.
- Pass `-DCJSH_MINIMAL_BUILD=ON` for the ultra-small binary profile.
- `cmake --build build --target clean` removes the build artifacts.
- Disable compile database emission with `-DCJSH_GENERATE_COMPILE_COMMANDS=OFF` if your tooling does not need it.
- Install the binary anywhere with `cmake --install build --config Release --prefix ~/.local` (adjust the prefix as desired).
- Export `CJSH_STRIP_BINARY=0` before configuring to keep symbols in non-Debug builds.

As before, git revision information is embedded automatically; use `CJSH_GIT_HASH_OVERRIDE` if you need to pin a custom value.

---

## Next Steps

Now that you have cjsh installed, check out [What You Need to Know](what-to-know.md) to learn about all the powerful features available out of the box and how to configure them to suit your workflow.
