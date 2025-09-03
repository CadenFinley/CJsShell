# All Features Plugin for CJSH

This plugin demonstrates all the features available in the CJ's Shell plugin system. It serves as a comprehensive example and reference implementation for plugin developers.

## Features Demonstrated

1. **Plugin Lifecycle**
   - Initialization and shutdown
   - State management with proper thread safety
   - Background thread processing

2. **Prompt Variables**
   - Registering custom prompt variables
   - Generating dynamic values for prompt variables
   - Using different types of prompt variables (time, quotes, etc.)

3. **Command Handling**
   - Registering multiple commands
   - Processing command arguments
   - Command help system

4. **Event Subscription**
   - Subscribing to shell events
   - Handling event notifications

5. **Settings Management**
   - Defining default settings
   - Handling setting updates
   - Applying settings to behavior

6. **Memory Management**
   - Proper allocation and deallocation
   - String and array handling
   - Thread-safe operation

## Prompt Variables

This plugin provides the following prompt variables for use in your themes:

- `{CURRENT_TIME}` - Displays the current system time
- `{PLUGIN_UPTIME}` - Shows how long the plugin has been running
- `{RANDOM_QUOTE}` - Shows a random inspirational quote

## Commands

The plugin provides these commands:

- `hello` - Print a greeting
- `echo [text]` - Echo back the provided text
- `settings` - Show current plugin settings
- `history` - Show command history
- `quote` - Show a random quote
- `time` - Show current time
- `uptime` - Show plugin uptime
- `help` - Show command help

## Settings

The following settings can be configured:

- `show_time_in_prompt` (true/false) - Whether to show time in the prompt
- `quote_refresh_interval` (seconds) - How often to refresh the random quote
- `enable_background_tasks` (true/false) - Whether to enable background processing

## Building and Installing

To build the plugin:

```bash
cd plugins/all_features_plugin
mkdir build
cd build
cmake ..
make
```

To install manually:

```bash
cp all_features_plugin.dylib ~/.config/cjsh/plugins/  # on macOS
# or
cp all_features_plugin.so ~/.config/cjsh/plugins/  # on Linux
```

## Usage with Themes

To use the plugin's prompt variables in your theme, add them to your theme's prompt format. For example:

```json
{
  "prompt_format": "[{CURRENT_TIME}] {PWD} $ ",
  "right_prompt_format": "{RANDOM_QUOTE} ({PLUGIN_UPTIME})"
}
```

## License

This plugin is provided as an example for educational purposes.
