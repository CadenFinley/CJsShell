# DevToolsTerminal

[![Build status](https://ci.appveyor.com/api/projects/status/dqk13klgh9d22bu5?svg=true)](https://ci.appveyor.com/project/CadenFinley/devtoolsterminal)
![Version](https://img.shields.io/github/v/release/CadenFinley/DevToolsTerminal?label=version&color=blue)
![Lines of Code](https://img.shields.io/badge/lines%20of%20code-8986-green)


DevToolsTerminal is a lightweight custom login shell designed for UNIX systems with integrated OpenAI capabilities. It can function as a complete login shell replacement or as a regular application. Beyond standard terminal functions, it allows users to execute terminal commands, create and manage shortcuts and multi-command scripts, configure startup commands, and interact with OpenAI's GPT models for enhanced productivity, syntax assistance, and error resolution.

## Using as a Login Shell

DevToolsTerminal can replace your system's default shell (bash, zsh, etc.) and function as a complete login environment. When used as a login shell, DevToolsTerminal:

- Initializes your environment by processing standard profile files (like /etc/profile and ~/.profile)
- Sets up proper PATH variables including Homebrew paths automatically
- Handles proper job control for background processes
- Manages signals and terminal settings appropriately
- Provides all standard shell functionality plus AI-powered assistance

This gives you a seamless experience where DevToolsTerminal launches automatically when you open your terminal or log in to your system.

## Installation

### Option 1: Using the Installation Script (macOS/Linux)

1. Use the one-line installation command:
   ```sh
   # Default installation
   curl -sL https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh | bash
   
   # Install AND set as default shell
   curl -sL https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh | bash -s -- --set-as-shell
   ```

   Or download and run the installation script manually:
   ```sh
   curl -O https://raw.githubusercontent.com/cadenfinley/DevToolsTerminal/main/tool-scripts/dtt-install.sh
   chmod +x dtt-install.sh
   
   # Standard installation
   ./dtt-install.sh
   
   # Install and set as default shell
   ./dtt-install.sh --set-as-shell
   ```

2. The script will:
   - Download the latest release from the specified source
   - Install to `~/.DTT-Data` directory and create a system-wide link in `/usr/local/bin`
   - Add DevToolsTerminal to `/etc/shells` as a valid login shell
   - Optionally set DevToolsTerminal as your default shell (with the `--set-as-shell` flag)
   - Make the application executable and install helper scripts

3. If you didn't set it as your default shell during installation, you can do so later with:
   ```sh
   chsh -s /usr/local/bin/DevToolsTerminal
   ```

4. Your original shell preference is automatically backed up to `~/.DTT-Data/original_shell.txt` when you switch

### Setting Back to Your Original Shell

If you want to revert to your previous shell:

```sh
# If you know your original shell
chsh -s /bin/zsh  # or /bin/bash, etc.

# Or use the backed-up shell path
chsh -s $(cat ~/.DTT-Data/original_shell.txt)
```

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
