# Contributing to CJ's Shell

Thank you for your interest in contributing to CJ's Shell (cjsh)! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
  - [Project Setup](#project-setup)
  - [Building the Project](#building-the-project)
- [How to Contribute](#how-to-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Suggesting Enhancements](#suggesting-enhancements)
  - [Pull Requests](#pull-requests)
- [Development Guidelines](#development-guidelines)
  - [Coding Style](#coding-style)
  - [Testing](#testing)
  - [Documentation](#documentation)
- [Project Structure](#project-structure)
- [Creating Plugins](#creating-plugins)
- [Creating Themes](#creating-themes)

## Code of Conduct

By participating in this project, you are expected to uphold our Code of Conduct, which is to treat all contributors with respect and foster an inclusive environment.

## Getting Started

### Project Setup

1. Fork the repository on GitHub
2. Clone your fork to your local machine:
   ```bash
   git clone https://github.com/YOUR-USERNAME/CJsShell.git
   cd CJsShell
   ```
3. Add the original repository as an upstream remote:
   ```bash
   git remote add upstream https://github.com/CadenFinley/CJsShell.git
   ```

### Building the Project

To build the project locally:

```bash
    ./toolchain/build.sh
```

## How to Contribute

### Reporting Bugs

If you find a bug, please create an issue on the GitHub repository with the following information:

- A clear, descriptive title
- Steps to reproduce the issue
- Expected behavior versus actual behavior
- Your environment information (OS, shell version, etc.)
- Any relevant logs or error messages

### Suggesting Enhancements

We welcome suggestions for new features! Please include:

- A clear description of the feature
- Why it would be valuable to the project
- Any implementation ideas you may have

### Pull Requests

1. Create a new branch for your feature or bugfix:
   ```bash
   git checkout -b feature/your-feature-name
   ```
   or
   ```bash
   git checkout -b fix/issue-you-are-fixing
   ```

2. Make your changes with proper commits
3. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
4. Create a pull request to the `master` branch of the original repository
5. Include a description of your changes in the pull request

## Development Guidelines

### Coding Style

Please follow these guidelines when writing code:

- Use consistent indentation (spaces, not tabs)
- Keep lines to a reasonable length (80-120 characters)
- Write clear, descriptive comments
- Follow C++ best practices
- Use meaningful variable and function names

This project uses clang-format to enforce consistent code style. Before submitting a pull request, please run the following command from the project root to format your code:

```bash
    find . -name "*.cpp" -o -name "*.h" -o -name "*.c" -o -name "*.hpp" | grep -v "build/" | grep -v "CMakeFiles/" | grep -v "_deps/" | xargs clang-format -i
```

This will automatically format all source files according to the project's style guidelines.

### Testing

- Add tests for new features
- Ensure all tests pass before submitting a pull request
- Run existing tests with:
  ```bash
  cd build
  make test
  ```

### Documentation

- Update documentation for any changed functionality
- Add documentation for new features
- Include comments in your code for complex logic
- Update the README.md if necessary

## Project Structure

The repository is organized as follows:

- `/include`: Header files
- `/src`: Source files
- `/plugins`: Plugin system and example plugins
- `/themes`: Theme files
- `/tests`: Test files
- `/build`: Build output (not committed)
- `/toolchain`: Tool chain for cjsh


## Creating Plugins

CJ's Shell supports a powerful plugin system. To create a plugin:

1. Examine the existing plugins in the `/plugins` directory for reference
2. Plugins can be written in C, C++, or Rust using the provided examples
3. Use the Plugin API defined in `include/pluginapi.h`
4. Follow the plugin documentation in the `/plugins/README.md`

Example plugin structure:
```
my_plugin/
├── build.sh                # Required build script
├── src/
│   └── my_plugin.cpp
├── include/
│   └── pluginapi.h         # Copy of the latest plugin API header
└── README.md
```

Plugins must implement all required functions from the Plugin API, including:
- `plugin_get_info`
- `plugin_initialize`
- `plugin_shutdown`
- `plugin_handle_command`
- `plugin_get_commands`
- `plugin_get_subscribed_events`
- `plugin_get_default_settings`
- `plugin_update_setting`
- `plugin_free_memory`

## Creating Themes


## Contributing Themes


By following these steps, you help maintain the quality and consistency of themes in CJ's Shell. Thank you for your contribution!
