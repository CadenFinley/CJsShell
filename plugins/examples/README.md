# CJSH Plugin Examples

This directory contains example plugins for CJSH in various programming languages.

## Available Examples

- **C**: Simple plugin written in C
- **C++**: Object-oriented plugin using C++
- **Rust**: Plugin using Rust FFI
- **Python**: Plugin wrapping Python code
- **Lua**: Plugin using Lua scripting
- **Node.js**: Plugin using Node.js

## Building Examples

Each example includes build instructions in the comments or a build script.

### C Plugin
```bash
cd c
gcc -shared -fPIC -o hello_c.so hello_plugin.c
# On macOS use .dylib instead of .so
```

### C++ Plugin
```bash
cd cpp
g++ -std=c++11 -shared -fPIC -o hello_cpp.so hello_plugin.cpp
# On macOS use .dylib instead of .so
```

### Rust Plugin
```bash
cd rust
cargo build --release
# Binary will be in target/release/libhello_rust.so (or .dylib on macOS)
```

### Python Plugin
```bash
cd python
python hello_plugin.py
# This will generate and compile the C wrapper
```

### Lua Plugin
```bash
cd lua
lua hello_plugin.lua
# This will generate and compile the C wrapper
```

### Node.js Plugin
```bash
cd nodejs
npm install
node hello_plugin.js
# This will generate and compile the C wrapper
```

## Installing Plugins

Copy the compiled shared libraries to your CJSH plugins directory:

```bash
cp hello_*.so ~/.cjsh/plugins/
# On macOS use .dylib instead of .so
```

## Testing Plugins

After installation, start CJSH and the plugins should be automatically loaded.
Test each plugin by running its command:

```
cjsh> hello_c
Hello from C, world! (from C plugin)

cjsh> hello_cpp
Hello from C++, world! (from C++ plugin)

cjsh> hello_rust
Hello from Rust, world! (from Rust plugin)

cjsh> hello_python
Hello from Python, world! (from Python plugin)

cjsh> hello_lua
Hello from Lua, world! (from Lua plugin)

cjsh> hello_node
Hello from Node.js, world! (from Node.js plugin)
```

You can also pass arguments:

```
cjsh> hello_c arg1 arg2
Hello from C, world! (from C plugin)
You provided arguments: arg1 arg2
```

## Changing Settings

Each plugin has a "greeting" setting that can be modified:

```
cjsh> set hello_c.greeting "Howdy"
cjsh> hello_c
Howdy, world! (from C plugin)
```
