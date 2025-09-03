# All Features Rust Plugin for CJSH

This is a Rust implementation of the "All Features Plugin" for CJ's Shell (CJSH). It demonstrates how to create CJSH plugins in Rust, showcasing the flexibility of the CJSH plugin API.

## Features

This plugin implements all the same functionality as the C++ version, including:

- Multiple commands: hello, echo, settings, history, quote, time, uptime, help
- Event subscription
- Settings management
- Background tasks
- Prompt variables

## Building with Cargo (Recommended)

If you have Rust and Cargo installed, you can build the plugin using:

```bash
# From the plugin directory
cargo build --release
```

The compiled library will be located at `target/release/liball_features_rust_plugin.dylib` (macOS) or `target/release/liball_features_rust_plugin.so` (Linux).

### Troubleshooting Build Issues

If you encounter linking errors for `plugin_register_prompt_variable`, there are a few possible solutions:

1. The plugin uses a weak symbol for this function that will be resolved at runtime.
2. The build configuration uses `-Wl,-undefined,dynamic_lookup` on macOS and `-Wl,--allow-shlib-undefined` on Linux to handle this.
3. Make sure your Rust compiler is up to date.

## Building with Makefile

For systems without Cargo, a Makefile is provided:

```bash
# From the plugin directory
make
```

Note: The Makefile requires `rustc` to be installed, along with the necessary dependencies.

## Installation

To install the plugin:

### Using Cargo

```bash
# From the plugin directory
cargo build --release
mkdir -p ~/.config/cjsh/plugins
cp target/release/liball_features_rust_plugin.* ~/.config/cjsh/plugins/
```

### Using Makefile

```bash
# From the plugin directory
make install
```

## Usage

Once installed, the plugin can be enabled in CJSH using:

```
plugin enable all_features_rust_plugin
```

### Available Commands

- `hello` - Prints a greeting
- `echo [text]` - Echoes back the provided text
- `settings` - Shows current plugin settings
- `history` - Shows command history
- `quote` - Shows a random quote
- `time` - Shows current time
- `uptime` - Shows plugin uptime
- `help` - Shows help message

### Prompt Variables

This plugin adds the following prompt variables that can be used in your CJSH prompt:

- `{CURRENT_TIME_RUST}` - Current system time
- `{PLUGIN_UPTIME_RUST}` - Time since plugin was initialized
- `{RANDOM_QUOTE_RUST}` - A random inspirational quote

## License

Same as CJSH.
