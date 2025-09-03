# CJ's Shell Plugin System

This directory contains plugins for CJ's Shell (CJSH). Plugins extend the functionality of the shell by adding new commands, prompt variables, and event handlers.

## Plugin Structure

Each plugin should be organized in its own directory with the following structure:

```
plugins/
├── my_plugin/                  # Plugin directory (name should be descriptive)
│   ├── build.sh                # Required build script
│   ├── src/                    # Source code files
│   │   ├── my_plugin.c         # Main plugin implementation
│   │   └── ...
│   ├── include/                # Header files (optional)
│   │   ├── pluginapi.h         # Latest plugin API header (REQUIRED)
│   │   └── ...
│   ├── README.md               # Plugin documentation (recommended)
│   └── my_plugin.so/dylib      # Compiled plugin (output of build.sh)
```

**IMPORTANT:** You must bundle the latest version of `pluginapi.h` with your plugin in its directory. This ensures compatibility even if the plugin API changes in future versions of CJSH. Your plugin will be compiled against the version of the API header you include with it, not the one in the system.

## Creating a Plugin

### Requirements

- Plugins must be compiled as shared libraries (`.so` on Linux, `.dylib` on macOS)
- C or C++ programming language
- The plugin must implement all required functions from the Plugin API
- A `build.sh` script must be included to build the plugin
- The latest `pluginapi.h` header file must be bundled with the plugin in its `include` directory

### Preparing Your Plugin Directory

1. Create your plugin directory in the `plugins/` folder
2. Create the required subdirectories (src, include)
3. Copy the latest `pluginapi.h` from the main `include/` directory to your plugin's `include/` directory:
   ```bash
   mkdir -p my_plugin/include
   cp /path/to/cjsh/include/pluginapi.h my_plugin/include/
   ```
4. Implement your plugin source files in the `src/` directory
5. Create the `build.sh` script as described below

### Required Plugin Functions

All plugins must implement the following functions:

| Function | Description |
|----------|-------------|
| `plugin_get_info` | Returns basic plugin information |
| `plugin_initialize` | Called when the plugin is enabled |
| `plugin_shutdown` | Called when the plugin is disabled |
| `plugin_handle_command` | Handles commands registered by this plugin |
| `plugin_get_commands` | Returns list of commands this plugin provides |
| `plugin_get_subscribed_events` | Returns list of events this plugin listens for |
| `plugin_get_default_settings` | Returns default plugin settings |
| `plugin_update_setting` | Called when a plugin setting is changed |
| `plugin_free_memory` | Frees memory allocated by the plugin |

### Memory Management Requirements

- All arrays and strings returned by `plugin_get_commands()`, `plugin_get_subscribed_events()`, and `plugin_get_default_settings()` MUST be heap-allocated (using `malloc`, `calloc`, etc.)
- The shell will call `plugin_free_memory()` to free this memory
- Never return static arrays or strings from these functions

### Example Plugin Implementation

```c
#include "pluginapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Basic plugin information
PLUGIN_API plugin_info_t* plugin_get_info() {
    static plugin_info_t info = {
        "example_plugin",         // name
        "1.0.0",                  // version
        "An example plugin",      // description
        "Your Name",              // author
        PLUGIN_INTERFACE_VERSION  // Must match current interface version
    };
    return &info;
}

// Initialize plugin
PLUGIN_API int plugin_initialize() {
    printf("Example plugin initialized\n");
    return PLUGIN_SUCCESS;
}

// Cleanup when plugin is disabled
PLUGIN_API void plugin_shutdown() {
    printf("Example plugin shutdown\n");
}

// Handle commands
PLUGIN_API int plugin_handle_command(plugin_args_t* args) {
    if (args->count > 0) {
        if (strcmp(args->args[0], "hello") == 0) {
            printf("Hello from example plugin!\n");
            return PLUGIN_SUCCESS;
        }
    }
    return PLUGIN_ERROR_INVALID_ARGS;
}

// List commands provided by this plugin
PLUGIN_API char** plugin_get_commands(int* count) {
    *count = 1;
    char** commands = (char**)malloc(sizeof(char*) * (*count));
    commands[0] = strdup("hello");
    return commands;
}

// List events this plugin subscribes to
PLUGIN_API char** plugin_get_subscribed_events(int* count) {
    *count = 1;
    char** events = (char**)malloc(sizeof(char*) * (*count));
    events[0] = strdup("main_process_start");
    return events;
}

// Default plugin settings
PLUGIN_API plugin_setting_t* plugin_get_default_settings(int* count) {
    *count = 1;
    plugin_setting_t* settings = (plugin_setting_t*)malloc(sizeof(plugin_setting_t) * (*count));
    settings[0].key = strdup("greeting");
    settings[0].value = strdup("Hello, World!");
    return settings;
}

// Handle setting updates
PLUGIN_API int plugin_update_setting(const char* key, const char* value) {
    if (strcmp(key, "greeting") == 0) {
        // Update internal setting
        return PLUGIN_SUCCESS;
    }
    return PLUGIN_ERROR_INVALID_ARGS;
}

// Free memory allocated by plugin
PLUGIN_API void plugin_free_memory(void* ptr) {
    free(ptr);
}
```

### The `build.sh` Script Requirement

Every plugin MUST include a `build.sh` script that:
1. Compiles the plugin source code
2. Creates the appropriate shared library (`.so` on Linux, `.dylib` on macOS)
3. Places the compiled plugin in the plugin's own directory

Example `build.sh` script:

```bash
#!/bin/bash
# Example build script for a CJSH plugin

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PLUGIN_NAME=$(basename "$SCRIPT_DIR")

# Detect OS for correct file extension
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    FILE_EXT="dylib"
    EXTRA_FLAGS="-undefined dynamic_lookup"
else
    # Linux
    FILE_EXT="so"
    EXTRA_FLAGS=""
fi

# Create build directory if it doesn't exist
mkdir -p "$SCRIPT_DIR/build"

# Compile the plugin
gcc -Wall -fPIC -shared $EXTRA_FLAGS \
    -I"$SCRIPT_DIR/include" \
    -I"$SCRIPT_DIR/../../include" \
    -o "$SCRIPT_DIR/$PLUGIN_NAME.$FILE_EXT" \
    "$SCRIPT_DIR/src/$PLUGIN_NAME.c"

echo "Plugin built: $SCRIPT_DIR/$PLUGIN_NAME.$FILE_EXT"
```

## Available Features

### Commands

Plugins can register custom commands that users can execute in the shell. When a user enters a command that your plugin has registered, the shell will call your plugin's `plugin_handle_command()` function.

### Events

Plugins can subscribe to various shell events to perform actions at specific points in the shell's lifecycle:

| Event | Description |
|-------|-------------|
| `main_process_pre_run` | Beginning of main process loop |
| `main_process_start` | Beginning of command processing loop |
| `main_process_end` | End of command processing loop |
| `main_process_command_processed` | After a command is processed |
| `plugin_enabled` | When a plugin is enabled |
| `plugin_disabled` | When a plugin is disabled |

### Prompt Variables

Plugins can register custom prompt variables that can be used in shell prompts. To register a prompt variable:

```c
plugin_string_t get_my_variable() {
    plugin_string_t result;
    result.data = strdup("my-value");
    result.length = strlen(result.data);
    return result;
}

// Call during plugin initialization
plugin_register_prompt_variable("my_var", get_my_variable);
```

Users can then use `{my_var}` in their prompt themes.

### Settings

Plugins can have configurable settings that users can modify. Define default settings with `plugin_get_default_settings()` and handle changes with `plugin_update_setting()`.

## Supported Languages

The primary supported language for plugins is C/C++. The plugin API is designed with C compatibility in mind, but C++ can be used with `extern "C"` declarations.

## Installing Plugins

To install a plugin:

1. Place the plugin directory in `~/.config/cjsh/plugins/` or in this plugins directory
2. Run the plugin's `build.sh` script to compile it
3. Restart CJSH or use the `plugin install` command

## Enabling and Disabling Plugins

- Enable a plugin: `plugin enable <plugin_name>`
- Disable a plugin: `plugin disable <plugin_name>`
- List available plugins: `plugin available`
- List enabled plugins: `plugin enabled`
- Enable all plugins: `plugin enableall`
- Disable all plugins: `plugin disableall`
- Remove a plugin: `plugin uninstall <plugin_name>`
- View plugin details: `plugin info <plugin_name>`
- List plugin commands: `plugin commands <plugin_name>`
- Configure plugin: `plugin settings <plugin_name>`

## Debugging Plugins

If your plugin isn't loading properly:

1. Check for compile errors in the build script output
2. Ensure all required functions are implemented
3. Verify that the interface version in `plugin_get_info()` matches `PLUGIN_INTERFACE_VERSION`
4. Check for runtime errors in the shell's output

## Example Plugins

See the `example_plugins` directory for working examples of plugins.

## API Reference

For detailed API documentation, refer to the `pluginapi.h` header file in the `include` directory.
