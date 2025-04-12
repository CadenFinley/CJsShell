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
   ```

   Or download and run the installation script manually:
   ```sh
   curl -O https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh
   chmod +x dtt-install.sh
   ./dtt-install.sh
   ```

2. The script will:
   - Download the latest release from the specified source
   - Install to `~/.DTT-Data` directory
   - Configure auto-launch with zsh
   - Make the application executable

3. Restart your terminal

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

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
