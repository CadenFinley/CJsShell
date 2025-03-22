# DevToolsTerminal

![CodeRabbit Pull Request Reviews](https://img.shields.io/coderabbit/prs/github/CadenFinley/DevToolsTerminal?utm_source=oss&utm_medium=github&utm_campaign=CadenFinley%2FDevToolsTerminal&labelColor=171717&color=FF570A&link=https%3A%2F%2Fcoderabbit.ai&label=CodeRabbit+Reviews)

DevToolsTerminal is a lightweight terminal emulator designed for UNIX systems with integrated OpenAI capabilities. It allows users to execute terminal commands, create and manage shortcuts and multi-command scripts, configure startup commands, and interact with OpenAI's GPT models for enhanced productivity, syntax assistance, and error resolution.

## Table of Contents
- [Features](#features)
- [Installation](#installation)
- [Command Format](#command-format)
- [Commands Reference](#commands-reference)
  - [Basic Commands](#basic-commands)
  - [Update Management](#update-management)
  - [Shortcuts](#shortcuts)
  - [User Settings](#user-settings)
    - [Startup Commands](#startup-commands)
    - [Shortcuts Management](#shortcuts-management)
    - [Multi-Command Shortcuts](#multi-command-shortcuts)
    - [Text and Display Settings](#text-and-display-settings)
    - [Data Management](#data-management)
    - [Testing](#testing)
  - [Theme Management](#theme-management)
  - [Environment Variables](#environment-variables)
- [AI Integration](#ai-integration)
  - [Core AI Commands](#core-ai-commands)
  - [AI Configuration](#ai-configuration)
  - [AI File Integration](#ai-file-integration)
  - [AI Chat Management](#ai-chat-management)
  - [OpenAI Integration Details](#openai-integration-details)
- [Plugin System](#plugin-system)
  - [Plugin Lifecycle](#plugin-lifecycle)
  - [Plugin Events](#plugin-events)
  - [Plugin Development Notes](#plugin-development-notes)
  - [Safety Considerations](#safety-considerations)
- [Terminal Features](#terminal-features)
  - [Color Interface](#color-interface)
  - [Git Integration](#git-integration)
  - [Multi-line Editing](#multi-line-editing)
  - [Cross-Platform Support](#cross-platform-support)
- [Data Storage](#data-storage)
- [Contributing](#contributing)
- [License](#license)
- [Author](#author)

## Features

- **Terminal Integration**: Execute terminal commands directly within the application with proper directory tracking
- **Shortcut Management**: Create, edit, and execute single and multi-command shortcuts for repetitive tasks
- **AI Integration**: Connect with OpenAI's GPT models for AI-assisted development with multiple modes and models
- **Customization**: Configure settings, command prefixes, and startup behaviors
- **Theme Management**: Customize terminal colors and save/load themes
- **Multi-line Input**: Support for entering multi-line commands with full cursor navigation
- **Git Integration**: Automatic Git repository detection with branch display in prompt
- **Data Persistence**: Save and load user preferences, command history, and chat contexts

## Installation

### Option 1: Using the Installation Script (macOS/Linux)

1. Use the one-line installation command:
   ```sh
   curl -sL https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh | bash
   ```

   Or download and run the installation script manually:
   ```sh
   curl -O https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh
   chmod +x dtt-install.sh
   ./dtt-install.sh
   ```

2. The script will:
   - Download the latest release from GitHub
   - Install to `/usr/local/bin` (or `~/.local/bin` if permission denied)
   - Configure auto-launch with zsh
   - Make the application executable

3. Restart your terminal or run:
   ```sh
   source ~/.zshrc
   ```

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

## Command Format

Commands in DevToolsTerminal follow a consistent format:

1. **Command Prefix**: By default, commands start with `!` (customizable via `!user text commandprefix [char]`)
2. **Command Structure**: `!category subcategory action [arguments]`
   - Example: `!user shortcut add myshortcut ls -la`
3. **Direct Terminal Commands**: Any input without the command prefix is sent directly to the terminal
4. **AI Mode**: When in AI mode (toggled via `!user text defaultentry ai`), input without command prefix is sent to ChatGPT

### Command Types
- **Simple Commands**: Single word commands like `!help`, `!exit`, `!clear`
- **Category Commands**: Multi-level commands like `!user startup add [command]`
- **Shortcut Commands**: Execute defined shortcuts via `!ss [shortcut]` or `!mm [shortcut]`

## Commands Reference

### Basic Commands

| Command | Description |
|---------|-------------|
| `!help` | Display available commands |
| `!exit` | Exit the application and save data |
| `!clear` | Clear screen and terminal cache |
| `!approot` | Navigate to application data directory |
| `!terminal [command]` | Execute terminal command directly |
| `!version` | Display application version information |

### Update Management

| Command | Description |
|---------|-------------|
| `!version` | Display current application version |
| `!checkforupdates` | Manually check for available updates |
| `!user checkforupdates enable` | Enable automatic update checking on startup |
| `!user checkforupdates disable` | Disable automatic update checking on startup |

The application automatically checks for updates on startup (if enabled) and will:
1. Compare local version with latest GitHub release
2. Prompt to download if an update is available
3. Display changelog after installing update

### Shortcuts

| Command | Description |
|---------|-------------|
| `!ss [shortcut]` | Execute a single-command shortcut |
| `!mm [shortcut]` | Execute a multi-command shortcut |

### User Settings

#### Startup Commands
| Command | Description |
|---------|-------------|
| `!user startup add [command]` | Add command to startup |
| `!user startup remove [command]` | Remove command from startup |
| `!user startup clear` | Clear all startup commands |
| `!user startup enable` | Enable startup commands |
| `!user startup disable` | Disable startup commands |
| `!user startup list` | List all startup commands |
| `!user startup runall` | Run all startup commands now |

#### Shortcuts Management
| Command | Description |
|---------|-------------|
| `!user shortcut add [shortcut] [command]` | Add a shortcut |
| `!user shortcut remove [shortcut]` | Remove a shortcut |
| `!user shortcut clear` | Clear all shortcuts |
| `!user shortcut enable` | Enable shortcuts |
| `!user shortcut disable` | Disable shortcuts |
| `!user shortcut list` | List all shortcuts |

#### Multi-Command Shortcuts
| Command | Description |
|---------|-------------|
| `!user shortcut mm add [name] [cmd1] [cmd2]...` | Add multi-command shortcut |
| `!user shortcut mm remove [name]` | Remove multi-command shortcut |

#### Text and Display Settings
| Command | Description |
|---------|-------------|
| `!user text commandprefix [char]` | Set command prefix character (default: !) |
| `!user text displayfullpath enable` | Show full directory path |
| `!user text displayfullpath disable` | Show shortened directory path |
| `!user text defaultentry ai` | Set default input mode to AI |
| `!user text defaultentry terminal` | Set default input mode to terminal |

#### Data Management
| Command | Description |
|---------|-------------|
| `!user data get userdata` | View user settings data |
| `!user data get userhistory` | View command history |
| `!user data get all` | View all user data |
| `!user data clear` | Clear all user data |
| `!user data saveloop enable` | Enable automatic data saving |
| `!user data saveloop disable` | Disable automatic data saving |
| `!user saveonexit enable` | Enable saving data on exit |
| `!user saveonexit disable` | Disable saving data on exit |
| `!user checkforupdates enable` | Enable automatic update checking |
| `!user checkforupdates disable` | Disable automatic update checking |

#### Testing
| Command | Description |
|---------|-------------|
| `!user testing enable` | Enable testing mode |
| `!user testing disable` | Disable testing mode |

### Theme Management

| Command | Description |
|---------|-------------|
| `!theme list` | List all available themes |
| `!theme load [name]` | Load a saved color theme |
| `!theme save [name]` | Save current colors as a theme |
| `!theme delete [name]` | Delete a saved theme |
| `!theme set [color] [value]` | Set a specific color in the current theme |
| `!theme current` | Display the name of the current theme |

The theme system allows you to customize and persist terminal color schemes:
- Themes are stored in the `.DTT-Data/themes` directory as JSON files
- Each theme includes settings for all terminal color variables
- The default theme is always available and cannot be deleted
- Colors affect various UI elements including prompts, messages, and highlights

#### Available Color Variables
The following color variables can be customized:
- `GREEN_COLOR_BOLD`: Used for success messages and AI interactions
- `RED_COLOR_BOLD`: Used for error messages and critical information
- `PURPLE_COLOR_BOLD`: Used for application branding and highlights
- `BLUE_COLOR_BOLD`: Used for directory names and information sections
- `YELLOW_COLOR_BOLD`: Used for warnings and branch names
- `CYAN_COLOR_BOLD`: Used for system messages and notifications
- `SHELL_COLOR`: Used for shell name in the prompt
- `DIRECTORY_COLOR`: Used for directory path in the prompt
- `BRANCH_COLOR`: Used for Git branch name in the prompt
- `GIT_COLOR`: Used for Git indicators in the prompt

Color values use standard ANSI escape codes (e.g., `\033[1;32m` for bold green).

### Environment Variables

| Command | Description |
|---------|-------------|
| `!env` | List all environment variables |
| `!env set NAME VALUE` | Set an environment variable |
| `!env get NAME` | Get value of an environment variable |
| `!env remove NAME` | Remove an environment variable |
| `!env unset NAME` | Alternative to remove |
| `!env clear` | Remove all environment variables |
| `!env expand STRING` | Show expansion of variables in a string |

Environment variables can be:
- Used in terminal commands with standard `$VAR` syntax
- Referenced with ${NAME} syntax for variables with special characters
- Persisted across sessions
- Used in shortcuts and startup commands
- Referenced in AI conversations for context
- Automatically expanded in commands before execution

## AI Integration

### Core AI Commands
| Command | Description |
|---------|-------------|
| `!ai` | Enter AI chat mode |
| `!ai help` | Display AI command help |
| `!ai chat [message]` | Send message to ChatGPT |
| `!aihelp` | Get AI help for recent terminal errors |

### AI Configuration
| Command | Description |
|---------|-------------|
| `!ai apikey set [KEY]` | Set OpenAI API key |
| `!ai apikey get` | Display current API key |
| `!ai model [model]` | Set AI model (default: gpt-3.5-turbo) |
| `!ai mode [mode]` | Set assistant type (chat, file-search, or code-interpreter) |
| `!ai timeoutflag [seconds]` | Set timeout for AI responses |
| `!ai directory set` | Set directory for AI-generated files to current directory |
| `!ai directory clear` | Reset directory to default (.DTT-Data) |

### AI File Integration
| Command | Description |
|---------|-------------|
| `!ai file add [file]` | Add file to AI context |
| `!ai file add all` | Add all files in directory to AI context |
| `!ai file remove [file]` | Remove file from AI context |
| `!ai file remove all` | Remove all files from AI context |
| `!ai file active` | List active files in AI context |
| `!ai file available` | List files available in current directory |
| `!ai file refresh` | Refresh active file contents |
| `!ai file clear` | Clear all files from context |

### AI Chat Management
| Command | Description |
|---------|-------------|
| `!ai chat history clear` | Clear chat history |
| `!ai chat cache enable` | Enable chat cache |
| `!ai chat cache disable` | Disable chat cache |
| `!ai chat cache clear` | Clear chat cache |
| `!ai log` | Log last AI conversation to file |
| `!ai get [key]` | Get specific response data |
| `!ai dump` | Dump complete response data |
| `!ai rejectchanges` | Reject AI-suggested changes |

### OpenAI Integration Details

#### Assistant Types
The application supports three OpenAI assistant modes:

1. **chat**: Standard conversational assistant
2. **file-search**: Assistant that analyzes provided files to help with related queries
3. **code-interpreter**: Advanced mode that can receive, modify, and create code files

#### Code Interpreter Mode
When using the code-interpreter mode, the assistant can:
- Read existing code files
- Suggest changes to code files and automatically apply them (similar to Github Copilot)
- Create new files in the specified save directory (defaults to .DTT-Data)
- Generate automatic diffs showing changes made to files
- Process and intelligently merge changes with existing file content
- Format code blocks appropriately based on language
- Create nested directory structures for new files when paths contain "/"
- Allow rejecting changes via `!ai rejectchanges` command to restore original file state
- Track file changes with colored diff output (red for deletions, green for additions)
- Handle new file creation automatically when referenced in code blocks

#### File Context
Adding files to the AI context allows the assistant to:
- Reference specific code during conversations
- Understand project structure and dependencies
- Provide more relevant and accurate answers to code-related questions
- Refresh file content with `!ai file refresh` to ensure latest changes are included

## Plugin System

| Command | Description |
|---------|-------------|
| `!plugin available` | List all available plugins |
| `!plugin enabled` | List all enabled plugins |
| `!plugin enableall` | Enable all available plugins |
| `!plugin disableall` | Disable all installed plugins |
| `!plugin settings` | Show settings for all plugins |
| `!plugin [name] enable` | Enable specific plugin |
| `!plugin [name] disable` | Disable specific plugin |
| `!plugin [name] info` | Get information about a plugin |
| `!plugin [name] commands` | List commands provided by a plugin |
| `!plugin [name] settings set [key] [value]` | Change plugin setting |
| `!plugin install [path]` | Install a new plugin from file |
| `!plugin uninstall [name]` | Remove an installed plugin |

Plugins provide extended functionality to DevToolsTerminal:
- Plugins are stored in the `.DTT-Data/plugins` directory
- Each plugin can define its own commands and settings
- Plugin state (enabled/disabled) persists across sessions
- Settings are managed per-plugin and saved with user data

### Plugin Lifecycle

Plugins follow a specific lifecycle:
1. **Discovery**: On startup, the application scans the plugins directory for compatible files (.so/.dylib)
2. **Loading**: Plugin files are dynamically loaded but remain inactive
3. **Initialization**: When enabled, the plugin's `initialize()` method is called
4. **Execution**: The plugin can handle commands and respond to events
5. **Shutdown**: When disabled, the plugin's `shutdown()` method is called
6. **Unloading**: On application exit or plugin uninstallation, resources are released

### Plugin Events

Plugins can respond to various system events:
- `event main_process pre_run`: Before the main process loop starts
- `event main_process start`: When the main process loop begins
- `event main_process took_input`: When user input is received
- `event main_process command_processed`: After a command is processed
- `event main_process end`: When the main process loop ends
- `event plugin_enabled [name]`: When a plugin is enabled
- `event plugin_disabled [name]`: When a plugin is disabled

### Plugin Development Notes

For developers interested in creating plugins:
- Plugins must implement the `PluginInterface` class
- Required methods include getName(), getVersion(), getDescription(), getAuthor()
- Plugins should properly handle initialization and shutdown
- Command handling is done through the handleCommand() method with a queue of arguments
- Plugins can define their own settings with default values
- Ensure plugins are compiled as shared libraries (.so on Linux, .dylib on macOS)
- Use the IMPLEMENT_PLUGIN macro to define required entry points

### Safety Considerations

When working with plugins:
- Only install plugins from trusted sources
- Plugins have full access to the terminal and system
- Disable or uninstall plugins that exhibit unexpected behavior
- The terminal will display a safety reminder when plugins are loaded

## Terminal Features

### Color Interface
The terminal features color-coded interface elements:
- Green: AI interactions and successful operations
- Red: Error messages and development mode indicators
- Purple: Application branding and highlights
- Default: Standard command input and output

### Git Integration
The terminal prompt automatically detects Git repositories and shows:
- Current directory name (or full path if enabled)
- Git branch name when inside a repository
- Color-coded repository and branch information
- Automatically traverses parent directories to find Git repositories
- Visual distinction between Git and non-Git directories in prompt

### Multi-line Editing
The terminal supports full multi-line editing capabilities:
- Arrow key navigation (up, down, left, right)
- Command history navigation
- Backspace handling across lines
- Proper cursor positioning

## Data Storage

User settings, command history, AI chat history, theme files, and code-interpreter generated files are stored in the `.DTT-Data` directory within your application directory:

- `.USER_DATA.json`: Contains user settings, shortcuts, API keys, and chat cache
- `.USER_COMMAND_HISTORY.txt`: Stores the history of all commands entered
- `themes/`: Directory containing saved color theme files
- `plugins/`: Directory containing user plugins
- Additional directories created by code-interpreter for generated files

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request with your enhancements.

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
