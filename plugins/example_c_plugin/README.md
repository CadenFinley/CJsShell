# Example C Plugin for CJ's Shell

This is a simple example plugin for CJ's Shell (CJSH) written in C. It demonstrates the basic functionality of the CJSH plugin system, including:

- Command registration and handling
- Event subscription
- Plugin settings
- Prompt variables
- Proper memory management

## Features

The plugin provides the following commands:

- `hello` - Displays a greeting message
- `counter` - Shows how many commands have been executed
- `uptime` - Shows the plugin's uptime in seconds
- `echo` - Echoes back all arguments
- `theme` - Gets or sets the current theme
- `help` - Shows a help message with all available commands

## Prompt Variables

The plugin provides the following prompt variables that can be used in your shell prompt:

- `{CMD_COUNT}` - The number of commands executed by this plugin
- `{PLUGIN_UPTIME}` - The plugin's uptime in seconds
- `{CURRENT_THEME}` - The current theme set in the plugin

## Settings

The plugin has the following configurable settings:

- `default_theme` - The default theme for the plugin (default: "default")
- `display_command_count` - Whether to display the command count (default: "true")

## Installation

1. Make sure the plugin directory is in your CJSH plugins path:
   ```bash
   mkdir -p ~/.config/cjsh/plugins/example_c_plugin
   cp -r . ~/.config/cjsh/plugins/example_c_plugin/
   ```

2. Build the plugin:
   ```bash
   cd ~/.config/cjsh/plugins/example_c_plugin
   chmod +x build.sh
   ./build.sh
   ```

3. Enable the plugin in CJSH:
   ```bash
   plugin enable example_c_plugin
   ```

## Usage

After enabling the plugin, you can use any of the commands listed above.

Examples:
```bash
hello
counter
uptime
echo Hello World
theme dark
theme
help
```

## License

Same as CJ's Shell
