# DevToolsTerminal-LITE

DevToolsTerminal OUTDATED!! Go see https://github.com/CadenFinley/DevToolsTerminal

## Features

- Execute terminal commands from within the application.
- Maintain a history of user inputs and terminal outputs.
- Support for shortcuts to quickly execute frequently used commands.
- Startup commands that run automatically when the application starts.
- Toggle display of the full path or just the current directory name in the terminal prompt.

## Dependencies

- C++17 or later
- [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing

## Installation

1. Clone the repository:
    ```sh
    git clone https://github.com/cadenfinley/termpasscpp.git
    cd termpasscpp
    ```

2. Ensure you have a C++17 compatible compiler.

3. Install the [nlohmann/json](https://github.com/nlohmann/json) library.

4. Build the project:
    ```sh
    mkdir build
    cd build
    cmake ..
    make
    ```

## Usage

Run the application:
```sh
./DevToolsTerminal-LITE
```

### Commands

- `!ss [ARGS]`: Process shortcuts.
- `!approot`: Go to the application directory.
- `!clear`: Clear the screen and terminal cache.
- `!ai`: AI-related commands (not supported in this build).
- `!user`: User settings commands.
- `!terminal [ARGS]`: Execute terminal commands.
- `!exit`: Exit the application.
- `!help`: Display help information.

### User Settings Commands

- `!user startup [ARGS]`: Manage startup commands.
- `!user text [ARGS]`: Manage text settings.
- `!user shortcut [ARGS]`: Manage shortcuts.
- `!user testing [ARGS]`: Enable or disable testing mode.
- `!user data [ARGS]`: Manage user data.

### Example

```sh
DevToolsTerminal LITE - Caden Finley (c) 2025
Created 2025 @ Abilene Christian University
Loading...
bash: /home/user git:(main)
> !ls
> !user startup add (echo "Hello, World!")
> !user startup list
```

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
