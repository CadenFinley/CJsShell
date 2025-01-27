# DevToolsTerminal LITE

DevToolsTerminal LITE is a lightweight terminal emulator with integrated OpenAI capabilities. It allows users to execute terminal commands, create and manage multi script/command shortcuts, create and manage startup commands and scripts, and interact with OpenAI's GPT models for enhanced productivity, syntax and error help.

## Features

- Execute terminal commands
- Manage shortcuts and multi-script shortcuts
- Integrated OpenAI GPT-3.5-turbo for AI-assisted tasks
- Customizable settings and startup commands
- Save and load user data and command history

## Installation

1. Clone the repository:
    ```sh
    git clone https://github.com/cadenfinley/DevToolsTerminal-LITE.git
    cd DevToolsTerminal-LITE
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
    ./DevToolsTerminal-LITE
    ```

## Usage

### Basic Commands

- `!help` - Display available commands.
- `!exit` - Exit the application.
- `!clear` - Clear the screen and terminal cache.

### Terminal Commands

- `!terminal [command]` - Execute a terminal command.
- `!terminal cd [directory]` - Change the current directory.

### AI Commands

- `!ai` - Enter AI chat mode.
- `!ai chat [message]` - Send a message to OpenAI.
- `!ai apikey set [API_KEY]` - Set the OpenAI API key.
- `!ai apikey get` - Get the current OpenAI API key.
- `!ai log` - Log the last AI chat.
- `!ai get [key]` - Get specific response data from the last AI chat.
- `!ai dump` - Dump all response data from the last AI chat.

### User Settings Commands

- `!user startup add [command]` - Add a command to startup commands.
- `!user startup remove [command]` - Remove a command from startup commands.
- `!user startup clear` - Clear all startup commands.
- `!user startup enable` - Enable startup commands.
- `!user startup disable` - Disable startup commands.
- `!user startup list` - List all startup commands.
- `!user startup runall` - Run all startup commands.

### Shortcut Commands

- `!user shortcut add [shortcut] [command]` - Add a shortcut.
- `!user shortcut remove [shortcut]` - Remove a shortcut.
- `!user shortcut clear` - Clear all shortcuts.
- `!user shortcut enable` - Enable shortcuts.
- `!user shortcut disable` - Disable shortcuts.
- `!user shortcut list` - List all shortcuts.

### Multi-Script Shortcut Commands

- `!user shortcut mm add [shortcut] [command1] [command2] ...` - Add a multi-script shortcut.
- `!user shortcut mm remove [shortcut]` - Remove a multi-script shortcut.

### Text Commands

- `!user text commandprefix [prefix]` - Set the command prefix.
- `!user text displayfullpath enable` - Enable displaying the full path.
- `!user text displayfullpath disable` - Disable displaying the full path.
- `!user text defaultentry ai` - Set default text entry to AI.
- `!user text defaultentry terminal` - Set default text entry to terminal.

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request.

## Author

Caden Finley
