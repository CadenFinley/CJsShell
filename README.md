# DevToolsTerminal

[![Build status](https://ci.appveyor.com/api/projects/status/dqk13klgh9d22bu5?svg=true)](https://ci.appveyor.com/project/CadenFinley/devtoolsterminal)
![Version](https://img.shields.io/github/v/release/CadenFinley/DevToolsTerminal?label=version&color=blue)
![Lines of Code](https://img.shields.io/badge/lines%20of%20code-8986-green)


DevToolsTerminal is a lightweight terminal emulator designed for UNIX systems with integrated OpenAI capabilities. It allows users to execute terminal commands, create and manage shortcuts and multi-command scripts, configure startup commands, and interact with OpenAI's GPT models for enhanced productivity, syntax assistance, and error resolution.

## Installation

### Option 1: Using the Installation Script (macOS/Linux)

1. Use the one-line installation command:
   ```sh
   # Default installation (from GitHub)
   curl -sL https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh | bash
   
   # Specify download source (GitHub)
   curl -sL https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh | bash -s gh
   
   # Specify download source (cadenfinley.com)
   curl -sL https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh | bash -s cjf
   ```

   Or download and run the installation script manually:
   ```sh
   curl -O https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh
   chmod +x dtt-install.sh
   ./dtt-install.sh  # or use ./dtt-install.sh gh or ./dtt-install.sh cjf to specify source
   ```

2. You can specify the download source when running the installer:
   ```sh
   # Download from GitHub (default)
   ./dtt-install.sh gh
   
   # Download from cadenfinley.com
   ./dtt-install.sh cjf
   ```

3. The script will:
   - Download the latest release from the specified source
   - Install to `~/.DTT-Data` directory
   - Configure auto-launch with zsh
   - Make the application executable

4. Restart your terminal

### File Structure

DevToolsTerminal creates the following directory structure in your home folder:

```
~/.DTT-Data/                          # Main data directory
├── DevToolsTerminal                  # Main executable
├── .USER_DATA.json                   # User settings and preferences
├── .USER_COMMAND_HISTORY.txt         # History of executed commands
├── executables_cache.json            # Cache of available system executables
├── dtt-uninstall.sh                  # Uninstall script
├── latest_changelog.txt              # Most recent changelog (after updates)
├── themes/                           # Themes directory
│   ├── default.json                  # Default color theme
│   └── [user-themes].json            # User-created themes
└── plugins/                          # Plugins directory
    ├── example-plugin.so             # Example plugin (Unix/Linux)
    └── another-plugin.dylib          # Example plugin (macOS)
```

### Uninstallation

To uninstall DevToolsTerminal:

1. Use the uninstall command inside the app: `!uninstall`
   
2. The uninstaller will:
   - Remove the DevToolsTerminal executable
   - Remove auto-launch entries from your .zshrc file
   - Optionally remove all stored data in the ~/.DTT-Data directory
   - Clean up any legacy installations from previous versions


### Option 2: Manual build Installation

1. Clone the repository:
   ```sh
   git clone https://github.com/cadenfinley/DevToolsTerminal.git
   cd DevToolsTerminal
   ```

2. Build the project:
   ```sh
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Run the application:
   ```sh
   ./DevToolsTerminal
   ```

## Features

### Plugins

DevToolsTerminal supports a plugin system that allows for extending functionality.

#### Managing Plugins

```sh
# List available plugins
!plugin available

# List enabled plugins
!plugin enabled

# Enable a plugin
!plugin enable [PLUGIN_NAME]

# Disable a plugin
!plugin disable [PLUGIN_NAME]

# View plugin information
!plugin info [PLUGIN_NAME]

# List commands for a plugin
!plugin commands [PLUGIN_NAME]

# Install a new plugin
!plugin install [PATH_TO_PLUGIN_FILE]

# Uninstall a plugin
!plugin uninstall [PLUGIN_NAME]

# Update plugin settings
!plugin settings [PLUGIN_NAME] set [SETTING_NAME] [VALUE]
```

#### Creating Plugins

Plugins are shared libraries (`.so` or `.dylib` files) that implement the `PluginInterface`. They must export `createPlugin` and `destroyPlugin` functions and follow the required interface version. Please refer to the plugin interface when creating plugins.

### Themes

DevToolsTerminal supports customizable color themes for terminal display.

#### Managing Themes

```sh
# Load a theme
!theme load [THEME_NAME]

# Save current colors as a theme
!theme save [THEME_NAME]
```

#### Theme Components

Themes are stored as JSON files in the `~/.DTT-Data/themes` directory.

### Startup Sequence

When DevToolsTerminal starts, it performs the following operations:

1. Initialize and verify data directories (`~/.DTT-Data`)
2. Load user data asynchronously
3. Load plugins asynchronously
4. Load themes asynchronously
5. Check for updates (if enabled)
6. Execute startup commands (if configured)

#### Managing Startup Commands

```sh
# List startup commands
!user startup list

# Add a startup command
!user startup add [COMMAND]

# Remove a startup command
!user startup remove [COMMAND]

# Clear all startup commands
!user startup clear

# Enable/disable startup commands
!user startup enable
!user startup disable

# Execute all startup commands
!user startup runall
```

### Shortcuts and Aliases

#### Multi-Command Shortcuts

Create shortcuts that execute multiple commands in sequence:

```sh
# Add a shortcut
!user shortcut add [NAME] [COMMAND1] [COMMAND2]...

# Use a shortcut
-[NAME]

# List shortcuts
!user shortcut list

# Remove a shortcut
!user shortcut remove [NAME]
```

#### Aliases

Create command aliases:

```sh
# Add an alias
!user alias add [NAME] [COMMAND]

# List aliases
!user alias list

# Remove an alias
!user alias remove [NAME]
```

### OpenAI Integration

DevToolsTerminal integrates with OpenAI's API for AI-assisted terminal usage:

```sh
# Set API key
!ai apikey set [YOUR_API_KEY]

# Switch to AI chat mode
!ai

# Get help with errors
!aihelp

# Configure AI settings
!ai mode [chat|file-search|code-interpreter]
!ai model [MODEL_NAME]
```

#### AI Assistant Types

- **chat**: General conversation mode for assistance
- **file-search**: Analyze files and respond based on their contents
- **code-interpreter**: Edit and modify files directly with AI assistance

#### File Context Management

```sh
# Add a file to AI context
!ai file add [FILENAME]

# Add all files in current directory
!ai file add all

# Remove a file from context
!ai file remove [FILENAME]

# Show active context files
!ai file active

# Show available files in current directory
!ai file available

# Update file contents in context
!ai file refresh

# Clear all files from context
!ai file clear
```

#### Chat History Management

```sh
# View chat history
!ai chat history

# Clear chat history
!ai chat history clear

# Enable token caching for better context retention
!ai chat cache enable

# Disable token caching
!ai chat cache disable

# Clear all cached tokens
!ai chat cache clear
```

#### Code Interpreter Features

The code-interpreter mode allows AI to directly modify files:

```sh
# Set working directory for file operations
!ai directory set

# Reset to default directory
!ai directory clear

# Reject recent AI changes to files
!ai rejectchanges
```

#### Advanced AI Settings

```sh
# Set timeout duration for API calls
!ai timeoutflag [SECONDS]

# Save recent AI exchanges to file
!ai log

# View raw AI response data
!ai dump

# Extract specific data from response
!ai get [KEY]
```

### Built-in Commands

DevToolsTerminal includes many built-in commands for terminal operation and customization:

#### Core Commands
- `!help` - Display available commands
- `!version` - Display application version
- `!terminal [COMMAND]` - Execute system terminal commands or switch back to the terminal menu
- `!uninstall` - Uninstall the application
- `!refresh-commands` - Refresh the executable commands cache
- `!ai` - Switch to the ai menu

#### User Settings
- `!user text commandprefix [CHAR]` - Change command prefix (default: !)
- `!user text shortcutprefix [CHAR]` - Change shortcut prefix (default: -)
- `!user text displayfullpath enable/disable` - Toggle full path display
- `!user text defaultentry ai/terminal` - Set default input mode
- `!user testing enable/disable` - Toggle testing mode
- `!user saveloop enable/disable` - Toggle auto-save loop
- `!user saveonexit enable/disable` - Toggle save on exit
- `!user checkforupdates enable/disable` - Toggle update checking
- `!user updatepath github/cadenfinley` - Set update source

#### AI Commands
- `!ai log` - Save chat log to file
- `!ai chat history` - Show chat history
- `!ai chat history clear` - Clear chat history
- `!ai file add/remove/active/available/refresh/clear` - Manage context files
- `!ai directory set/clear` - Configure save directory

#### Navigation
- `!approot` - Go to application directory
- `!clear` - Clear terminal screen

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
